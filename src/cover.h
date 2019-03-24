void cover_initialize(HINSTANCE hInstance);
void cover_cleanup();
void coverRegion(int horiz_coord, int vert_coord);
void uncoverRegion(int horiz_coord, int vert_coord);
void update_window();
extern BYTE* restrict imageBits;
