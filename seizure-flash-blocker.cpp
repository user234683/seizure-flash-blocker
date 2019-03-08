//Headers
#include <Windows.h>
#include <SDKDDKVer.h>
#include <objidl.h>
#include <gdiplus.h>
#include <tlhelp32.h>
#include <string>
#include <stdio.h>
#include <vector>

//Libraries
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")


//Precompiled constants
#define NUM_FRAMES 6
#define THRESHOLD 14400LL  // threshold for a single change in an individual pixel, which if repeated is problematic

#define REGION_SIDELENGTH_PIXELS 50 // Side length in pixels of the square regions

// Just a compiler test variable to make sure it doesn't overflow, these numbers get big
const long long MAX_REGION_CHANGE = 255LL*255LL*REGION_SIDELENGTH_PIXELS*REGION_SIDELENGTH_PIXELS*NUM_FRAMES;

#define FRAME_RATE 30
#define CAPTURE_TIMER_ID 12345 // arbitrary number

//Structs
typedef struct {
	bool bad;
	int frames_last_set;            // How many frames ago this was set as bad
	long long total_change;      // aggregate color distance change for all pixels in region over last NUM_FRAMES frames
	long long changes[NUM_FRAMES - 1];    // circular buffer of aggregate color distance changes between each frame in the region.
} RegionStatus;


//Class name for registering windows class. Name is arbitrary.
std::wstring window_className = L"OverlayWindowClass";
//Startup input for GDI+
GdiplusStartupInput gdiplusStartupInput;
//ID of Graphics Device Interface+
ULONG_PTR           gdiplusToken;
//Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
//Handle Device Context
HDC hDC;
//Handlw Window
HWND hWnd;

int ScreenX = 0;
int ScreenY = 0;
int windowOffsetX;
int windowOffsetY;

// Stores the last NUM_FRAMES screen captures as a circular buffer
// Screens_start stores the index of the start of this circular buffer (the index of the oldest capture)
// Each capture is of length FRAME_SIZE (4*ScreenX*ScreenY)
// The 4 is because there are 4 bytes per pixel, BGR and maybe A or something
// screens itself is a 1D array acting as a 2D array, due to C limitations.
BYTE* screens;
int FRAME_SIZE;
int screens_start = 0;

int HORIZ_REGIONS;     // integers calculated based on actual screen size
int HORIZ_REMAINDER;
int VERT_REGIONS;
int VERT_REMAINDER;

long long TOTAL_REGION_THRESHOLD;    // threshold for aggregate change for an entire region over NUM_FRAMES frames
long long TOTAL_REGION_THRESHOLD_BOTTOM;
long long TOTAL_REGION_THRESHOLD_RIGHT;
long long TOTAL_REGION_THRESHOLD_BOTTOM_RIGHT;

RegionStatus* regions;
int changes_start;         // start index of the changes circular buffer in RegionStatus


// These are the lists of regions to cover or uncover in the WM_PAINT event
std::vector<RECT> to_cover;
std::vector<RECT> to_uncover;

//Window variables
HWND captureWindow;
HWND foundWindow;
HDC hdcScreen;
HDC hdcScreenCopy;
HDC hdcBitmap;
HBITMAP hBitmap;
BITMAPINFOHEADER bmi = { 0 };

//Important functions since c++ is a forward declaration language that reads from top to bottom
void InitializeWindow(HINSTANCE hInstance);
void RegisterWindowClass(HINSTANCE hInstance);
void GDI_Init();
int smallestFactorGreaterThan(int x, int y);
//Cover the region with a color to prevent epliepsy
void coverRegion(int horiz_coord, int vert_coord);
//Uncover the region with a color when there is no rapid color change.
void uncoverRegion(int horiz_coord, int vert_coord);
//Get R/G/B colors in that pixel
inline int PosB(int i, int x, int y);
inline int PosG(int i, int x, int y);
inline int PosR(int i, int x, int y);
//Find window handle of chrome
HWND findChrome();
//Iterate through all processors ran on this computer.
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM fuckcpp);
//Update a new frame
void newFrame();
int euclidean_modulus(int a, int b);

void InitializeWindow(HINSTANCE hInstance) {

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
void GDI_Init() {
	// Initialize GDI+. This allows us to draw alpha pixels

	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}
void RegisterWindowClass(HINSTANCE hInstance) {
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

// finds the smallest factor of x greater than y
int smallestFactorGreaterThan(int x, int y) {
	int factor = y;
	while (x % factor != 0) {     // sorry I dont know number theory
		factor++;
	}
	return factor;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM fuckcpp) {
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
HWND findChrome() {
	EnumWindows((WNDENUMPROC)&EnumWindowsProc, 0);
	return foundWindow;
}
void initialize() {
	//Get the window position and size of a Chrome Window
	//Some of the chrome processors may be service or even nonsense. We will take the process ID
	//and window handle that contains the visible window of Chrome
	captureWindow = findChrome();
	hdcScreen = GetWindowDC(captureWindow);
	RECT windowRect;
	GetWindowRect(captureWindow, &windowRect);
	ScreenX = windowRect.right - windowRect.left;
	ScreenY = windowRect.bottom - windowRect.top;

	FRAME_SIZE = 4*ScreenX*ScreenY;

	windowOffsetX = windowRect.left;
	windowOffsetY = windowRect.top;

	//Split the window into regions
	HORIZ_REMAINDER = ScreenX % REGION_SIDELENGTH_PIXELS;
	HORIZ_REGIONS = ScreenX / REGION_SIDELENGTH_PIXELS + (HORIZ_REMAINDER == 0 ? 0 : 1);

	VERT_REMAINDER = ScreenY % REGION_SIDELENGTH_PIXELS;
	VERT_REGIONS = ScreenY / REGION_SIDELENGTH_PIXELS + (VERT_REMAINDER == 0 ? 0 : 1);

	TOTAL_REGION_THRESHOLD = THRESHOLD * REGION_SIDELENGTH_PIXELS * REGION_SIDELENGTH_PIXELS * NUM_FRAMES;
	// For the special regions to the right of the screen that don't quite fit
	TOTAL_REGION_THRESHOLD_RIGHT = THRESHOLD * HORIZ_REMAINDER * REGION_SIDELENGTH_PIXELS * NUM_FRAMES;
	// ditto but for the bottom
	TOTAL_REGION_THRESHOLD_BOTTOM = THRESHOLD * REGION_SIDELENGTH_PIXELS * VERT_REMAINDER * NUM_FRAMES;
	// ditto but for bottom right
	TOTAL_REGION_THRESHOLD_BOTTOM_RIGHT = THRESHOLD * HORIZ_REMAINDER * VERT_REMAINDER * NUM_FRAMES;

	hdcScreenCopy = CreateCompatibleDC(hdcScreen);
	hdcBitmap = CreateCompatibleDC(hdcScreen);
	hBitmap = CreateCompatibleBitmap(hdcScreen, ScreenX, ScreenY);
	SelectObject(hdcBitmap, hBitmap);

	// allocate memory for screens and regions
	screens = new BYTE[NUM_FRAMES*FRAME_SIZE];
	regions = new RegionStatus[HORIZ_REGIONS*VERT_REGIONS];

	//Do a worst case allocation by preparing a premade size of a vector
	to_cover.reserve(HORIZ_REGIONS*VERT_REGIONS);
	to_uncover.reserve(HORIZ_REGIONS*VERT_REGIONS);

	//bitmap to be copied to the screen
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biPlanes = 1;
	bmi.biBitCount = 32;
	bmi.biWidth = ScreenX;
	bmi.biHeight = -ScreenY;
	bmi.biCompression = BI_RGB;
	bmi.biSizeImage = 0;

}
void cleanup() {
	//Delete anything pointers or resource that we use
	if (screens) {
		delete screens;
	}
	if (regions) {
		delete regions;
	}

	ReleaseDC(GetDesktopWindow(), hdcScreen);
	DeleteDC(hdcScreenCopy);
	DeleteDC(hdcBitmap);
	DeleteObject(hBitmap);


	GdiplusShutdown(gdiplusToken);
}

// https://stackoverflow.com/questions/16112482/c-getting-rgb-from-hbitmap
void ScreenCap(unsigned int i) {
	if (i >= NUM_FRAMES)     // ensure never out of bounds
		return;

	BitBlt(hdcBitmap, 0, 0, ScreenX, ScreenY, hdcScreen, 0, 0, SRCCOPY);
	GetDIBits(hdcScreenCopy, hBitmap, 0, ScreenY, &screens[i*FRAME_SIZE], (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
}

inline int PosB(int i, int x, int y)
{
	return screens[i*FRAME_SIZE + 4 * ((y*ScreenX) + x)];
}

inline int PosG(int i, int x, int y)
{
	return screens[i*FRAME_SIZE + 4 * ((y*ScreenX) + x) + 1];
}

inline int PosR(int i, int x, int y)
{
	return screens[i*FRAME_SIZE + 4 * ((y*ScreenX) + x) + 2];
}

int euclidean_modulus(int a, int b) {
	int m = a % b;
	if (m < 0)
		m = m + b;
	return m;
}

inline RECT get_region_rect(int horiz_coord, int vert_coord) {
	RECT rect;
	rect.top = vert_coord * REGION_SIDELENGTH_PIXELS + windowOffsetY;
	rect.left = horiz_coord * REGION_SIDELENGTH_PIXELS + windowOffsetX;
	rect.bottom = rect.top + (vert_coord == VERT_REGIONS - 1 ? VERT_REMAINDER : REGION_SIDELENGTH_PIXELS);
	rect.right = rect.left + (horiz_coord == HORIZ_REGIONS - 1 ? HORIZ_REMAINDER : REGION_SIDELENGTH_PIXELS);
    return rect;
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

inline void analyzeRegion(int prev_frame_i, int new_frame_i, long long region_threshold,
                          int horiz_r, int vert_r,
                          int region_width, int region_height) {


    //In this region, whenever we add a new frame and remove a oldest frame
    RegionStatus* region = &regions[horiz_r*VERT_REGIONS + vert_r];
    //We subtract the oldest aggregate change from the oldest frame
    region->total_change -= region->changes[changes_start];
    long long new_change = 0;
    //This will compute the aggregate change of the vectors of each pixel RGB within the frame
    for (int x_i = 0; x_i < region_width; x_i++) {
        int x = horiz_r * REGION_SIDELENGTH_PIXELS + x_i;
        for (int y_i = 0; y_i < region_height; y_i++) {
            int y = vert_r * REGION_SIDELENGTH_PIXELS + y_i;

            int deltaB = PosB(new_frame_i, x, y) - PosB(prev_frame_i, x, y);
            int deltaG = PosG(new_frame_i, x, y) - PosG(prev_frame_i, x, y);
            int deltaR = PosR(new_frame_i, x, y) - PosR(prev_frame_i, x, y);

            new_change += deltaB * deltaB;
            new_change += deltaG * deltaG;
            new_change += deltaR * deltaR;
        }
    }

    region->changes[changes_start] = new_change;
    //We then add the newest frame. By doing this, instead of adding all the NUM_FRAMES frames which costs
    //NUM_FRAMES operations we only use 2 operations regardless of NUM_FRAMES frames we have
    region->total_change += new_change;

    // check if the changes is extreme. The aggregate change is very high
    if (region->total_change > region_threshold) {
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
void newFrame() {
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
}

//Every X seconds, we will call a new frame which depends on the FPS we have set.
void timerCallback(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime) {
	newFrame();
}

//Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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



int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR str, int nCmdShow) {

	MSG msg = { 0 };
	BOOL bRet;
	//Perform init things
	initialize();
	GDI_Init();
	InitializeWindow(hInstance);

	// capture the screen at 10 fps
	SetTimer(hWnd, CAPTURE_TIMER_ID, (int)(1.0f / FRAME_RATE * 1000), (TIMERPROC)&timerCallback);


	//Main Message loop that will iterate over and over until it receives a destroy message
	//in that case it is a ID of -1 and will close the app
	while (::GetMessageW(&msg, NULL, 0, 0) > 0)
	{
		::TranslateMessage(&msg);
		::DispatchMessageW(&msg);
	}
	//clean up the stuff
	cleanup();
	return (int)msg.wParam;
}
