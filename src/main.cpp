/*
main.cpp:
 - entry point
 - sets up event loop
 - sets up timer that will call screencap at FRAME_RATE defined in constants.h
 - calls the initialization and cleanup functions for each module

newframe.cpp: captures, stores, and analyzes the screen, tells cover.cpp when to cover/uncover regions

cover.cpp: Manages the covering window and how to cover regions

screeninfo.cpp:
 - At startup, gets info about the "screen" (capture window at the moment)
 - At startup, divides the screen into regions
 - Makes this information available to other modules


Module hierarchy:

-----------------------------------
|               main              |
-----------------------------------
 |            | init,          | init
 |            | screencap      |
 |            V                V
 |       ------------      ---------
 |       | newframe |      | cover |
 |       ------------      ---------
 |            |                |
 | init       | screen dims,   | screen dimensions,
 |            | region divs    | region divisions
 V            V                V
-----------------------------------
|           screeninfo            |
-----------------------------------


Additionally, every module gets the #define'd constants from constants.h
*/



#include <windows.h>
#include "constants.h"
#include "screeninfo.h"
#include "cover.h"
#include "newframe.h"


#define CAPTURE_TIMER_ID 12345 // arbitrary number


//Every X seconds, we will call a new frame which depends on the FPS we have set.
void timerCallback(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime) {
	newframe();
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR str, int nCmdShow) {

	MSG msg = { 0 };

	// Module initialization. Order is important
    screeninfo_initialize();
	cover_initialize(hInstance);
    newframe_initialize();


	// capture the screen at FRAME_RATE fps
	SetTimer(NULL, CAPTURE_TIMER_ID, (int)(1.0f / FRAME_RATE * 1000), (TIMERPROC)&timerCallback);


	//Main Message loop that will iterate over and over until it receives a destroy message
	//in that case it is a ID of -1 and will close the app
	while (::GetMessageW(&msg, NULL, 0, 0) > 0)
	{
		::TranslateMessage(&msg);
		::DispatchMessageW(&msg);
	}

	//Module cleanup
	newframe_cleanup();
    cover_cleanup();
    screeninfo_cleanup();

	return (int)msg.wParam;
}
