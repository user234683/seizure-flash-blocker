// newframe.cpp: captures, stores, and analyzes the screen
#include <windows.h>
#include <stdbool.h>

#include "constants.h"
#include "newframe.h"
#include "screeninfo.h"
#include "cover.h"



// Just a compiler test variable to make sure it can't overflow, these numbers get big
const CHANGE_TYPE MAX_REGION_CHANGE = (CHANGE_TYPE)(1)*255*255*REGION_SIDELENGTH_PIXELS*REGION_SIDELENGTH_PIXELS*NUM_FRAMES;




// screen capturing stuff
BYTE* restrict screens;
int FRAME_SIZE;
int screens_start = 0;


HDC hdcScreenCopy;
HDC hdcBitmap;
HBITMAP hBitmap;
BITMAPINFOHEADER bmi = { 0 };





// analysis stuff

CHANGE_TYPE TOTAL_REGION_THRESHOLD;    // threshold for aggregate change for an entire region between two frames
CHANGE_TYPE TOTAL_REGION_THRESHOLD_BOTTOM;
CHANGE_TYPE TOTAL_REGION_THRESHOLD_RIGHT;
CHANGE_TYPE TOTAL_REGION_THRESHOLD_BOTTOM_RIGHT;


#define CHANGES_LENGTH (NUM_FRAMES - 1)

// https://stackoverflow.com/questions/4079243/how-can-i-use-sizeof-in-a-preprocessor-macro
#define BITARRAY_TYPE long
_Static_assert(CHANGES_LENGTH < sizeof(BITARRAY_TYPE)*8, "NUM_FRAMES is too long");

typedef struct {
    bool bad;
    int frames_last_set;                // How many frames ago this was set as bad
    char number_of_violations;           // running total of threshold_violations
    BITARRAY_TYPE threshold_violations; // a bit array. The Nth bit is 1 if the inter-frame change N frames ago was too much
} RegionStatus;

RegionStatus* restrict regions;






void newframe_initialize(){

    regions = (RegionStatus*) calloc(HORIZ_REGIONS*VERT_REGIONS, sizeof(RegionStatus));

    TOTAL_REGION_THRESHOLD = (CHANGE_TYPE)(1)*THRESHOLD * REGION_SIDELENGTH_PIXELS * REGION_SIDELENGTH_PIXELS;
    // For the special regions to the right of the screen that don't quite fit
    TOTAL_REGION_THRESHOLD_RIGHT = (CHANGE_TYPE)(1)*THRESHOLD * HORIZ_REMAINDER * REGION_SIDELENGTH_PIXELS;
    // ditto but for the bottom
    TOTAL_REGION_THRESHOLD_BOTTOM = (CHANGE_TYPE)(1)*THRESHOLD * REGION_SIDELENGTH_PIXELS * VERT_REMAINDER;
    // ditto but for bottom right
    TOTAL_REGION_THRESHOLD_BOTTOM_RIGHT = (CHANGE_TYPE)(1)*THRESHOLD * HORIZ_REMAINDER * VERT_REMAINDER;



    

    FRAME_SIZE = 4*ScreenX*ScreenY;
    screens = (BYTE*) malloc(NUM_FRAMES*FRAME_SIZE);


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
        free(screens);

    if(regions)
        free(regions);


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








// returns whether the badness of the region has changed
static inline bool analyzeRegion(int prev_frame_i, int new_frame_i, CHANGE_TYPE region_threshold,
                          int horiz_r, int vert_r,
                          int region_width, int region_height) {


    //In this region, whenever we add a new frame and remove a oldest frame
    RegionStatus* restrict region = &regions[vert_r*HORIZ_REGIONS + horiz_r];
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


    region->threshold_violations <<= 1;
    if(new_change > region_threshold){
        region->threshold_violations |= 1;
        region->number_of_violations++;
    }
    if(region->threshold_violations & (1 << CHANGES_LENGTH )){
        region->number_of_violations--;
    }
    region->threshold_violations ^= (1 << CHANGES_LENGTH );

    if (region->number_of_violations >= NUM_FRAMES - 3) {
        region->frames_last_set = 0;
        if (!region->bad) {
            region->bad = true;
            coverRegion(horiz_r, vert_r);
            return true; // Bad was false, now true
        }
    }
    else {
        // If not remove the cover and consider the region safe
        region->frames_last_set++;
        if (region->bad && region->frames_last_set >= NUM_FRAMES) {
            region->bad = false;
            uncoverRegion(horiz_r, vert_r);
            return true; // Bad was true, now false
        }
    }
    return false; // No change in bad status
}


static bool analyzeByRegion(int prev_frame_i, int new_frame_i) {
    bool needs_update = false;
    //The following finds the aggregate distance of all region.
    for (int vert_r = 0; vert_r < VERT_REGIONS - 1; vert_r++) {
        for (int horiz_r = 0; horiz_r < HORIZ_REGIONS - 1; horiz_r++) {

            // general case
            needs_update |= analyzeRegion(prev_frame_i, new_frame_i, TOTAL_REGION_THRESHOLD,
                                          horiz_r,                  vert_r,
                                          REGION_SIDELENGTH_PIXELS, REGION_SIDELENGTH_PIXELS);
        }
        // first special case: regions at right of screen
        needs_update |= analyzeRegion(prev_frame_i, new_frame_i, TOTAL_REGION_THRESHOLD_RIGHT,
                                      HORIZ_REGIONS - 1,    vert_r,
                                      HORIZ_REMAINDER,      REGION_SIDELENGTH_PIXELS);
    }

    for (int horiz_r = 0; horiz_r < HORIZ_REGIONS - 1; horiz_r++) {
        // second special case: regions at bottom of screen
        needs_update |= analyzeRegion(prev_frame_i, new_frame_i, TOTAL_REGION_THRESHOLD_BOTTOM,
                                      horiz_r,                  VERT_REGIONS - 1,
                                      REGION_SIDELENGTH_PIXELS, VERT_REMAINDER);
    }

    // third special case: region at bottom right of screen
    needs_update |= analyzeRegion(prev_frame_i, new_frame_i, TOTAL_REGION_THRESHOLD_BOTTOM_RIGHT,
                                  HORIZ_REGIONS - 1,    VERT_REGIONS - 1,
                                  HORIZ_REMAINDER,      VERT_REMAINDER);

    return needs_update;
}



static bool analyzeByPixels(int prev_frame_i, int new_frame_i){
    bool needs_update = false;
    for(int y = 0; y < ScreenY; y++){
        for(int x = 0; x < ScreenX; x++){
            // special case where a "region" is just a single pixel
            RegionStatus* restrict region = &regions[y*ScreenX + x];
            int new_change = 0;
            int deltaB = PosB(new_frame_i, x, y) - PosB(prev_frame_i, x, y);
            int deltaG = PosG(new_frame_i, x, y) - PosG(prev_frame_i, x, y);
            int deltaR = PosR(new_frame_i, x, y) - PosR(prev_frame_i, x, y);

            new_change += deltaB * deltaB;
            new_change += deltaG * deltaG;
            new_change += deltaR * deltaR;

            region->threshold_violations <<= 1;
            if(new_change > THRESHOLD){
                region->threshold_violations |= 1;
                region->number_of_violations++;
            }
            if(region->threshold_violations & (1 << CHANGES_LENGTH )){
                region->number_of_violations--;
            }
            region->threshold_violations ^= (1 << CHANGES_LENGTH );

            if (region->number_of_violations >= NUM_FRAMES - 3) {
                region->frames_last_set = 0;
                if (!region->bad) {
                    region->bad = true;
                    memset(&imageBits[4*(y*ScreenX + x)], 255, 4);
                    needs_update = true; // Bad was false, now true
                }
            }
            else {
                // If not remove the cover and consider the region safe
                region->frames_last_set++;
                if (region->bad && region->frames_last_set >= NUM_FRAMES) {
                    region->bad = false;
                    memset(&imageBits[4*(y*ScreenX + x)], 0, 4);
                    needs_update = true; // Bad was true, now false
                }
            }
        }
    }
    return needs_update;
}




//BYTE screens[NUM_FRAMES][4 * ScreenX*ScreenY];
//unsigned int screens_start = 0;
// https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nc-winuser-timerproc
// https://stackoverflow.com/questions/15685095/how-to-use-settimer-api
void newframe() {
    // true if any of the regions have had their bad status changed, and thus we need to redraw the bitmap
    bool needs_update = false;

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

#if REGION_SIDELENGTH_PIXELS == 1
    needs_update = analyzeByPixels(prev_frame_i, new_frame_i);
#else
    needs_update = analyzeByRegion(prev_frame_i, new_frame_i);
#endif

    if(needs_update)
        update_window();
}


