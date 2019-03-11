// cover.cpp: Manages the covering window and how to cover regions
#include <windows.h>
#include <SDKDDKVer.h>
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include <string>

#include "constants.h"
#include "cover.h"
#include "screeninfo.h"

using namespace Gdiplus;


// These are the lists of regions to cover or uncover in the WM_PAINT event
std::vector<RECT> to_cover;
std::vector<RECT> to_uncover;



//Class name for registering windows class. Name is arbitrary.
std::wstring window_className = L"OverlayWindowClass";
//Startup input for GDI+
GdiplusStartupInput gdiplusStartupInput;
//ID of Graphics Device Interface+
ULONG_PTR           gdiplusToken;
//Windows procedure
static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
//Handle Device Context
HDC hDC;
//Handlw Window
HWND hWnd;

static void RegisterWindowClass(HINSTANCE hInstance) {
	window_className = L"Class1";
	// Register window class
	WNDCLASSEXW wcex = { 0 };
	wcex.cbSize = sizeof(wcex);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = ::LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)::GetStockObject(BLACK_BRUSH);
	wcex.lpszClassName = window_className.c_str();
	::RegisterClassExW(&wcex);
}

static void InitializeWindow(HINSTANCE hInstance) {

	RegisterWindowClass(hInstance);


	//The following gets the monitor width and height so that our app can cover the entire monitor.
	HWND desktop_hwnd = GetDesktopWindow();
	RECT desktop;
	GetWindowRect(desktop_hwnd, &desktop);
	LONG horizontal = desktop.right;
	LONG vertical = desktop.bottom;

	//Create a window top most to the top left and remove the window in the toolbar upon creation.
	hWnd = ::CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW, //Keep window on top from all other windows and hide the windows from toolbar
													//Our window should not have any borders at all
		window_className.c_str(), //Class Name
		L"", //Window name-  We don't care about this because we won't have a window border that displays the name or neither the 
			//toolbar showing this app
		WS_POPUP | WS_VISIBLE, //Visible pop up
		CW_USEDEFAULT, CW_USEDEFAULT, //Default x and y position for creating windows 
		horizontal, vertical, //Fully fill the entire monitor screne.
		NULL, //There is no parent window 
		NULL, //We are not gonna use a menu for this app
		hInstance, //Handle instance (obtained from WinMain)
		NULL); //This is used if we are using win32 api as a class. In this case this is useless for us;


	// Make window semi-transparent, and mask out background color
	::SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_ALPHA | LWA_COLORKEY);
	//Display the window but we will not have a border and transparent background.
	::ShowWindow(hWnd, WS_VISIBLE);
	//This will paint the window through WM_PAINT in windows procedure
	::UpdateWindow(hWnd);

	//This code will disable user input (mouse clicking and keyboard) and allow user activity to work below this application.
	//These graphics will render last meaning that all windows will render first and then this window to ensure
	//the drawn graphics appears on the top.
	LONG cur_style = GetWindowLong(hWnd, GWL_EXSTYLE);
	SetWindowLong(hWnd, GWL_EXSTYLE, cur_style | WS_EX_TRANSPARENT | WS_EX_LAYERED);

	//(Not related to the objective but) Set the Timer every 50ms to perform invalidations to paintinng through WM_PAINT.
	//Code handling occurs in DRAW_TIMER at the WM_TIMER at windows procedure.
	//SetTimer(hWnd, DRAW_TIMER, 500, (TIMERPROC)NULL);

}


//Window procedure for cover window
static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		//This case is responsible for drawing colors and other stuff on our window.
	case WM_PAINT:
	{
		if (to_cover.size() == 0 && to_uncover.size() == 0)
			return ::DefWindowProc(hWnd, message, wParam, lParam);  // Don't waste resources setting up painting

		PAINTSTRUCT ps = { 0 };
		//Begin Paint
		hDC = ::BeginPaint(hWnd, &ps);

		Graphics graphics(hDC);
		SolidBrush uncover_pen(Color(255, 0, 0, 0));
		SolidBrush cover_pen(Color(255, 255, 1, 1));
        RECT rect;
		for (int i = 0; i < to_cover.size(); i++) {
            rect = to_cover[i];
            // cast because the rect uses longs and compiler doesn't know whether to cast to float or int in this overloaded function
			graphics.FillRectangle(&cover_pen, (int)rect.left, (int)rect.top, (int)(rect.right - rect.left), (int)(rect.bottom - rect.top));
		}
		for (int i = 0; i < to_uncover.size(); i++) {
            rect = to_uncover[i];
			graphics.FillRectangle(&uncover_pen, (int)rect.left, (int)rect.top, (int)(rect.right - rect.left), (int)(rect.bottom - rect.top));
		}
		to_uncover.clear();
		to_cover.clear();

		//End paint.
		::EndPaint(hWnd, &ps);
		return 0;
	}

	case WM_DESTROY:
		//Destroy the window and output a 0 to end the application
		::PostQuitMessage(0);
		return 0;
	default:
		//Windows message that we don't care and let the operating system do it.
		break;
	}
	return ::DefWindowProc(hWnd, message, wParam, lParam);
}



void cover_initialize(HINSTANCE hInstance){
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	InitializeWindow(hInstance);

	//Do a worst case allocation by preparing a premade size of a vector
	to_cover.reserve(HORIZ_REGIONS*VERT_REGIONS);
	to_uncover.reserve(HORIZ_REGIONS*VERT_REGIONS);
}

void cover_cleanup(){
    GdiplusShutdown(gdiplusToken);

}

void coverRegion(int horiz_coord, int vert_coord) {
	RECT rect = get_region_rect(horiz_coord, vert_coord);
	InvalidateRect(hWnd, &rect, 0);
	to_cover.push_back(rect);
}
void uncoverRegion(int horiz_coord, int vert_coord) {
	RECT rect = get_region_rect(horiz_coord, vert_coord);
	InvalidateRect(hWnd, &rect, 0);
	to_uncover.push_back(rect);
}