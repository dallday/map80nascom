/*
 * definitions for the status display.c
 *
 */
 
// colours for SDL_PIXELFORMAT_ARGB8888 
// https://majerle.eu/documentation/gui/html/group___g_u_i___c_o_l_o_r_s.html
 
#define STATUS_WHITE   0xFFFFFFFF
#define STATUS_BLUE    0xFF0000FF
#define STATUS_LIGHTBLUE    0xFF8080FF
#define STATUS_GREEN   0xFF00FF00
#define STATUS_RED     0xFFFF0000 
#define STATUS_BLACK   0xFF000000 
#define STATUS_ORANGE  0xFFFF7F00
#define STATUS_YELLOW  0xFFC8C800


#define STATUS_DISPLAYSCALEY       4    // sets the initial scaling of the display 
#define STATUS_DISPLAYSCALEX       4    //  2 means use 2 pixels both across and down on each character pixel
                                        // the -s option will set a actual scaling factor for the screen 
                                        // 

#define STATUS_DISPLAYCHARACTERS  48    
#define STATUS_DISPLAYLINES       16
#define STATUS_DISPLAYRAMSIZE     ( STATUS_DISPLAYCHARACTERS * STATUS_DISPLAYSCALEX * STATUS_DISPLAYLINES * STATUS_DISPLAYSCALEY )

#define STATUS_BYTESPERCHARACTER 16     // number of bytes in each character from the font file

// #define MAP80VFCDISPLAY_FONT_H       16			// number of pixels high 16
//#define MAP80VFCDISPLAY_FONT_H       10			// 
// actually only using 10 lines
#define STATUS_FONT_H       10		// number of pixels high 16
// actually 10 lines doubled

#define STATUS_FONT_W        8         // number of pixels wide

#define STATUS_DISPLAY_X_OFFSET 24 // 48     // number of pixels in left and right border
#define STATUS_DISPLAY_Y_OFFSET  8     // number of pixels in top and bottom border


//#define STATUS_DISPLAY_WIDTH   480*STATUS_DISPLAYSCALEX     // how many pixels the screen is wide  - 48 characters * 8 =  384 + ( 48 offset * 2 ) = 480
//#define STATUS_DISPLAY_HEIGHT  256*STATUS_DISPLAYSCALEY     // how many pixels the screen is high  - 16 line by 15 high = 240 + ( 8 offset *2 ) = 256

#define STATUS_DISPLAY_WIDTH  ((STATUS_DISPLAYCHARACTERS * STATUS_FONT_W * STATUS_DISPLAYSCALEX)+(STATUS_DISPLAY_X_OFFSET*2))
//    480*STATUS_DISPLAYSCALEX     // how many pixels the screen is wide  - 48 characters * 8 =  384 + ( 48 offset * 2 ) = 480

#define STATUS_DISPLAY_HEIGHT  ((STATUS_DISPLAYLINES*STATUS_FONT_H*STATUS_DISPLAYSCALEY)+(STATUS_DISPLAY_Y_OFFSET*2))
// 256*STATUS_DISPLAYSCALEY     // how many pixels the screen is high  - 16 line by 15 high = 240 + ( 8 offset *2 ) = 256

#define STATUS_DISPLAYFORCEREFRESH 10

int status_create_screen(BYTE *ScreenMemory);
void status_display_change_size(int sizefactor);
void status_display_position(int x, int y);
// get the current size of the nascom window on the screen
// updates the integers pointed to by w and h
void status_GetWindowSize(int* w, int* h);


void status_display_refresh(void);
void status_display_show_string(char * stringdata, unsigned int xpos, unsigned int ypos);
void status_display_show_string_full(char * stringdata, unsigned int xpixelpos, unsigned int ypixelpos, 
        unsigned fontxscale, unsigned fontyscale,  uint32_t charcolour, uint32_t backgroundcolour);
void status_display_show_chars(char * stringdata, unsigned int xcharpos, unsigned int ycharpos);
void status_display_show_chars_full(char * stringdata, unsigned int xcharpos, unsigned int ycharpos, 
        unsigned fontxscale, unsigned fontyscale,  uint32_t charcolour, uint32_t backgroundcolour);

// position of screen on computer screen
extern int statusdisplayxpos;
extern int statusdisplayypos;

// end of file



