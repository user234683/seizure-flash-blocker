// newframe.cpp: captures, stores, and analyzes the screen
#include <windows.h>

#include "constants.h"
#include "newframe.h"
#include "screeninfo.h"
#include "cover.h"



// Just a compiler test variable to make sure it can't overflow, these numbers get big
const CHANGE_TYPE MAX_REGION_CHANGE = (CHANGE_TYPE)(1)*255*255*REGION_SIDELENGTH_PIXELS*REGION_SIDELENGTH_PIXELS*NUM_FRAMES;




// screen capturing stuff
BYTE* screens;
int FRAME_SIZE;
int screens_start = 0;


HDC hdcScreen;
HDC hdcScreenCopy;
HDC hdcBitmap;
HBITMAP hBitmap;
BITMAPINFOHEADER bmi = { 0 };





// analysis stuff

CHANGE_TYPE TOTAL_REGION_THRESHOLD;    // threshold for aggregate change for an entire region between two frames
CHANGE_TYPE TOTAL_REGION_THRESHOLD_BOTTOM;
CHANGE_TYPE TOTAL_REGION_THRESHOLD_RIGHT;
CHANGE_TYPE TOTAL_REGION_THRESHOLD_BOTTOM_RIGHT;


#define CHANGES_LENGTH NUM_FRAMES -1
typedef struct {
    bool bad;
    int frames_last_set;            // How many frames ago this was set as bad
    CHANGE_TYPE changes[CHANGES_LENGTH];    // circular buffer of aggregate color distance changes between each frame in the region.
} RegionStatus;

RegionStatus* regions;
int changes_start;         // start index of the changes circular buffer in RegionStatus






void newframe_initialize(){

    regions = new RegionStatus[HORIZ_REGIONS*VERT_REGIONS];

    TOTAL_REGION_THRESHOLD = (CHANGE_TYPE)(1)*THRESHOLD * REGION_SIDELENGTH_PIXELS * REGION_SIDELENGTH_PIXELS;
    // For the special regions to the right of the screen that don't quite fit
    TOTAL_REGION_THRESHOLD_RIGHT = (CHANGE_TYPE)(1)*THRESHOLD * HORIZ_REMAINDER * REGION_SIDELENGTH_PIXELS;
    // ditto but for the bottom
    TOTAL_REGION_THRESHOLD_BOTTOM = (CHANGE_TYPE)(1)*THRESHOLD * REGION_SIDELENGTH_PIXELS * VERT_REMAINDER;
    // ditto but for bottom right
    TOTAL_REGION_THRESHOLD_BOTTOM_RIGHT = (CHANGE_TYPE)(1)*THRESHOLD * HORIZ_REMAINDER * VERT_REMAINDER;



    

    FRAME_SIZE = 4*ScreenX*ScreenY;
    screens = new BYTE[NUM_FRAMES*FRAME_SIZE];


    hdcScreen = GetWindowDC(captureWindow);
    hdcScreenCopy = CreateCompatibleDC(hdcScreen);
    hdcBitmap = CreateCompatibleDC(hdcScreen);
    hBitmap = CreateCompatibleBitmap(hdcScreen, ScreenX, ScreenY);
    SelectObject(hdcBitmap, hBitmap);


    //bitmap header indicating screencap bitmap format
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biWidth = ScreenX;
    bmi.biHeight = -ScreenY;
    bmi.biCompression = BI_RGB;
    bmi.biSizeImage = 0;

}

// https://stackoverflow.com/questions/16112482/c-getting-rgb-from-hbitmap
static void ScreenCap(unsigned int i) {
    if (i >= NUM_FRAMES)     // ensure never out of bounds
        return;

    BitBlt(hdcBitmap, 0, 0, ScreenX, ScreenY, hdcScreen, 0, 0, SRCCOPY);
    GetDIBits(hdcScreenCopy, hBitmap, 0, ScreenY, &screens[i*FRAME_SIZE], (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
}

void newframe_cleanup(){
    if(screens)
        delete screens;

    if(regions)
        delete regions;


    ReleaseDC(GetDesktopWindow(), hdcScreen);
    DeleteDC(hdcScreenCopy);
    DeleteDC(hdcBitmap);
    DeleteObject(hBitmap);
}

static inline int PosB(int i, int x, int y)
{
    return screens[i*FRAME_SIZE + 4 * ((y*ScreenX) + x)];
}

static inline int PosG(int i, int x, int y)
{
    return screens[i*FRAME_SIZE + 4 * ((y*ScreenX) + x) + 1];
}

static inline int PosR(int i, int x, int y)
{
    return screens[i*FRAME_SIZE + 4 * ((y*ScreenX) + x) + 2];
}









static inline void analyzeRegion(int prev_frame_i, int new_frame_i, CHANGE_TYPE region_threshold,
                          int horiz_r, int vert_r,
                          int region_width, int region_height) {


    //In this region, whenever we add a new frame and remove a oldest frame
    RegionStatus* region = &regions[horiz_r*VERT_REGIONS + vert_r];
    CHANGE_TYPE new_change = 0;
    //This will compute the aggregate change of the vectors of each pixel RGB within the frame
    for (int y_i = 0; y_i < region_height; y_i++) {
        int y = vert_r * REGION_SIDELENGTH_PIXELS + y_i;
        for (int x_i = 0; x_i < region_width; x_i++) {
            int x = horiz_r * REGION_SIDELENGTH_PIXELS + x_i;


            int deltaB = PosB(new_frame_i, x, y) - PosB(prev_frame_i, x, y);
            int deltaG = PosG(new_frame_i, x, y) - PosG(prev_frame_i, x, y);
            int deltaR = PosR(new_frame_i, x, y) - PosR(prev_frame_i, x, y);

            new_change += deltaB * deltaB;
            new_change += deltaG * deltaG;
            new_change += deltaR * deltaR;
        }
    }

    region->changes[changes_start] = new_change;

    int number_of_thresholds = 0;
    for(int i = 0; i < CHANGES_LENGTH; i++){
        number_of_thresholds += (region->changes[i] > region_threshold);

    }
    // check if the changes is extreme. The aggregate change is very high
    if (number_of_thresholds >= NUM_FRAMES - 3) {
        region->frames_last_set = 0;
        if (!region->bad) {
            //If so than this may/may not be a epilepsy and will cover it with a red color.
            region->bad = true;
            coverRegion(horiz_r, vert_r);
        }
    }
    else {
        // If not remove the cover and consider the region safe
        region->frames_last_set++;
        if (region->bad && region->frames_last_set >= NUM_FRAMES) {
            region->bad = false;
            uncoverRegion(horiz_r, vert_r);
        }
    }
}

//BYTE screens[NUM_FRAMES][4 * ScreenX*ScreenY];
//unsigned int screens_start = 0;
// https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nc-winuser-timerproc
// https://stackoverflow.com/questions/15685095/how-to-use-settimer-api
void newframe() {
    int new_frame_i = screens_start;

    //We have a (NUM_FRAMES) frames used for a circular buffer.
    //If there are free space in a buffer, we are able to allocate the frame for the buffer
    //Otherwise if the buffer is full, the oldest buffer will get replace with the new one.
    ScreenCap(screens_start);
    screens_start++;
    if (screens_start >= NUM_FRAMES) {
        screens_start = 0;
    }

    int prev_frame_i = new_frame_i - 1;
    if (prev_frame_i < 0)
        prev_frame_i += NUM_FRAMES;

    //The following finds the aggregate distance of all region.
    for (int horiz_r = 0; horiz_r < HORIZ_REGIONS - 1; horiz_r++) {
        for (int vert_r = 0; vert_r < VERT_REGIONS - 1; vert_r++) {
            // general case
            analyzeRegion(prev_frame_i, new_frame_i, TOTAL_REGION_THRESHOLD,
                          horiz_r,                  vert_r,
                          REGION_SIDELENGTH_PIXELS, REGION_SIDELENGTH_PIXELS);
        }
        // first special case: regions at bottom of screen
        analyzeRegion(prev_frame_i, new_frame_i, TOTAL_REGION_THRESHOLD_BOTTOM,
                      horiz_r,                  VERT_REGIONS - 1,
                      REGION_SIDELENGTH_PIXELS, VERT_REMAINDER);
    }

    for (int vert_r = 0; vert_r < VERT_REGIONS - 1; vert_r++) {
        // second special case: regions at right of screen
        analyzeRegion(prev_frame_i, new_frame_i, TOTAL_REGION_THRESHOLD_RIGHT,
                      HORIZ_REGIONS - 1,    vert_r, 
                      HORIZ_REMAINDER,      REGION_SIDELENGTH_PIXELS);
    }

    // third special case: region at bottom right of screen
    analyzeRegion(prev_frame_i, new_frame_i, TOTAL_REGION_THRESHOLD_BOTTOM_RIGHT,
                  HORIZ_REGIONS - 1,    VERT_REGIONS - 1, 
                  HORIZ_REMAINDER,      VERT_REMAINDER);

    // advance changes circular buffer
    changes_start++;
    if (changes_start >= CHANGES_LENGTH)
        changes_start = 0;

}


