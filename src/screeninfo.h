extern HWND captureWindow;
extern HDC hdcScreen;
extern int ScreenX;
extern int ScreenY;
extern int windowOffsetX;
extern int windowOffsetY;


extern int HORIZ_REGIONS;     // integers calculated based on actual screen size
extern int HORIZ_REMAINDER;
extern int VERT_REGIONS;
extern int VERT_REMAINDER;

RECT get_region_rect(int horiz_coord, int vert_coord);
RECT get_offset_region_rect(int horiz_coord, int vert_coord);

void screeninfo_initialize();
void screeninfo_cleanup();
