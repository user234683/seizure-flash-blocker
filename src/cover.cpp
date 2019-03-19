// cover.cpp: Manages the covering window and how to cover regions
#include <windows.h>
#include <SDKDDKVer.h>
#include <objidl.h>
#include <vector>
#include <string>

#include "constants.h"
#include "cover.h"
#include "screeninfo.h"



// Just {ScreenX, ScreenY}, required for a winapi call
SIZE screenSize;
// Just {windowOffsetX, windowOffsetY}
POINT screenOffset;

// Bitmap used as a mask to cover regions
HBITMAP coverBitmap = NULL;
// Array holding the bytes for the bitmap. Same format as screen captures
BYTE * imageBits = NULL;

// Device context that holds the bitmap mask
HDC coverDC;
// Old bitmap from coverDC, put it back after cleaning up
HBITMAP oldBitmap;

//Class name for registering windows class. Name is arbitrary.
const char* window_className = "OverlayWindowClass";
//Handle Device Context
HDC hDC;
//Handlw Window
HWND hWnd;

static void RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = window_className;
    RegisterClass(&wc);
}

static void InitializeWindow(HINSTANCE hInstance) {
    RegisterWindowClass(hInstance);

    //Create a window top most to the top left and remove the window in the toolbar upon creation.
    hWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW, //Keep window on top from all other windows and hide the windows from toolbar
                                                    //Our window should not have any borders at all
        window_className,       // Class Name
        "coverwindow",          // Window name
        WS_POPUP | WS_VISIBLE,          // Visible pop up
        CW_USEDEFAULT, CW_USEDEFAULT,   // Default x and y position for creating windows
        ScreenX, ScreenY,               // Same size as capture window
        NULL,                   // There is no parent window
        NULL,                   // Window doesn't need a menu
        hInstance,              // Handle instance (obtained from WinMain)
        NULL);                  // This is used if we are using win32 api as a class. In this case this is useless for us
}




void cover_initialize(HINSTANCE hInstance){
    InitializeWindow(hInstance);

    // http://faithlife.codes/blog/2008/09/displaying_a_splash_screen_with_c_part_i/

    // Prepare structure giving bitmap information (negative height indicates a top-down DIB)
    BITMAPINFO bminfo;
    ZeroMemory(&bminfo, sizeof(bminfo));
    bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bminfo.bmiHeader.biWidth = ScreenX;
    bminfo.bmiHeader.biHeight = -ScreenY;
    bminfo.bmiHeader.biPlanes = 1;
    bminfo.bmiHeader.biBitCount = 32;
    bminfo.bmiHeader.biCompression = BI_RGB;


    // Prepare the DIB (device independent bitmap) which acts as the covering mask
    coverBitmap = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, (VOID**) &imageBits, NULL, 0);

    // device context which holds the bitmap I guess
    coverDC = CreateCompatibleDC(hdcScreen);
    // select bitmap into the cover device context. Save old one to put it back during cleanup
    oldBitmap = (HBITMAP) SelectObject(coverDC, coverBitmap);

    // needed by UpdateLayeredWindow call
    screenSize = {ScreenX, ScreenY};
    screenOffset = {windowOffsetX, windowOffsetY};
}

void cover_cleanup(){
    SelectObject(coverDC, oldBitmap);
    DeleteDC(coverDC);
}

void coverRegion(int horiz_coord, int vert_coord) {
    // coordinates with top left corner of window as origin
    RECT rect = get_region_rect(horiz_coord, vert_coord);
    int region_width = rect.right - rect.left;

    for(int y = rect.top; y < rect.bottom; y++){
        memset(&imageBits[4*(y*ScreenX + rect.left)], 255, 4*region_width);
    }
}
void uncoverRegion(int horiz_coord, int vert_coord) {
    // coordinates with top left corner of window as origin
    RECT rect = get_region_rect(horiz_coord, vert_coord);
    int region_width = rect.right - rect.left;

    for(int y = rect.top; y < rect.bottom; y++){
        memset(&imageBits[4*(y*ScreenX + rect.left)], 0, 4*region_width);
    }
}

void update_window(){
    //http://faithlife.codes/blog/2008/09/displaying_a_splash_screen_with_c_part_ii/

    // use the source image's alpha channel for blending
    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT ptZero = { 0 };
    // paint the window (in the right location) with the alpha-blended bitmap
    UpdateLayeredWindow(hWnd, hdcScreen, &screenOffset, &screenSize,
        coverDC, &ptZero, RGB(0, 0, 0), &blend, ULW_ALPHA);
}

