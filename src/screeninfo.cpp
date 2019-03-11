// screeninfo.cpp: gets info about the screen and capture window, divides the screen into regions
#include <windows.h>
#include <tlhelp32.h>

#include "constants.h"
#include "screeninfo.h"


HWND captureWindow;
int ScreenX = 0;
int ScreenY = 0;
int windowOffsetX;
int windowOffsetY;


int HORIZ_REGIONS;     // integers calculated based on actual screen size
int HORIZ_REMAINDER;
int VERT_REGIONS;
int VERT_REMAINDER;




HWND foundWindow;   // can't figure out the casting for LPARAM right now
static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM fuckcpp) {
    if (!IsWindowVisible(hwnd))
        return TRUE;
    DWORD processID;
    //This function gets the processID that belongs to the windows handle.
    GetWindowThreadProcessId(hwnd, &processID);

    PROCESSENTRY32 peInfo;
    //Get a list of processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, processID);
    if (hSnapshot)
    {
        peInfo.dwSize = sizeof(peInfo);
        BOOL nextProcess = Process32First(hSnapshot, &peInfo);
        while (nextProcess)
        {
            //Check if the process is chrome.exe and has the right processID
            if (peInfo.th32ProcessID == processID && strcmp(peInfo.szExeFile, "chrome.exe") == 0)
            {
                //We got it. By returning false we stop iterating through our list of processes
                CloseHandle(hSnapshot);
                foundWindow = hwnd;
                return FALSE;
            }
            //Cycle through each processes from chrome.exe
            nextProcess = Process32Next(hSnapshot, &peInfo);
        }
        CloseHandle(hSnapshot);
    }
    //Search for more other processes for other checks
    return TRUE;

}
static HWND findChrome() {
    EnumWindows((WNDENUMPROC)&EnumWindowsProc, 0);
    return foundWindow;
}

void screeninfo_initialize() {
    //Get the window position and size of a Chrome Window
    //Some of the chrome processors may be service or even nonsense. We will take the process ID
    //and window handle that contains the visible window of Chrome
    captureWindow = findChrome();
    RECT windowRect;
    GetWindowRect(captureWindow, &windowRect);
    ScreenX = windowRect.right - windowRect.left;
    ScreenY = windowRect.bottom - windowRect.top;

    windowOffsetX = windowRect.left;
    windowOffsetY = windowRect.top;



    //Split the window into regions
    HORIZ_REMAINDER = ScreenX % REGION_SIDELENGTH_PIXELS;
    HORIZ_REGIONS = ScreenX / REGION_SIDELENGTH_PIXELS + (HORIZ_REMAINDER == 0 ? 0 : 1);

    VERT_REMAINDER = ScreenY % REGION_SIDELENGTH_PIXELS;
    VERT_REGIONS = ScreenY / REGION_SIDELENGTH_PIXELS + (VERT_REMAINDER == 0 ? 0 : 1);

}

void screeninfo_cleanup(){

}


RECT get_region_rect(int horiz_coord, int vert_coord) {
    RECT rect;
    rect.top = vert_coord * REGION_SIDELENGTH_PIXELS + windowOffsetY;
    rect.left = horiz_coord * REGION_SIDELENGTH_PIXELS + windowOffsetX;
    rect.bottom = rect.top + (vert_coord == VERT_REGIONS - 1 ? VERT_REMAINDER : REGION_SIDELENGTH_PIXELS);
    rect.right = rect.left + (horiz_coord == HORIZ_REGIONS - 1 ? HORIZ_REMAINDER : REGION_SIDELENGTH_PIXELS);
    return rect;
}






