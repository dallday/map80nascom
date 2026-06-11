/*
 * definitions for the display.c
 *
 */

#define NASCOM_DISPLAYSCALEY       2
#define NASCOM_DISPLAYSCALEX       2

#define NASCOM_DISPLAYCHARACTERS  48
#define NASCOM_DISPLAYLINES       16

//#define NASCOM_FONT_H_PITCH 16         // height of the characters in the font table - see font.c
#define NASCOM_BYTESPERCHARACTER 16     // number of bytes in each character from the font file

//#define NASCOM_FONT_H       15			// number of pixels high /* 15 or 14 - check docs */
//#define NASCOM_FONT_H       16			// number of pixels high /* 15 or 14 - check docs */
// it is actually 14 in the Nascom2 Hardware
#define NASCOM_FONT_H       14			// number of pixels high 14
#define NASCOM_FONT_W        8         // number of pixels wide

#define NASCOM_DISPLAY_X_OFFSET 24 // 48     // number of pixels in left and right border
#define NASCOM_DISPLAY_Y_OFFSET  8     // number of pixels in top and bottom border


//#define NASCOM_DISPLAY_WIDTH   480*NASCOM_DISPLAYSCALEX     // how many pixels the screen is wide  - 48 characters * 8 =  384 + ( 48 offset * 2 ) = 480
//#define NASCOM_DISPLAY_HEIGHT  256*NASCOM_DISPLAYSCALEY     // how many pixels the screen is high  - 16 line by 15 high = 240 + ( 8 offset *2 ) = 256

#define NASCOM_DISPLAY_WIDTH  ((NASCOM_DISPLAYCHARACTERS * NASCOM_FONT_W * NASCOM_DISPLAYSCALEX)+(NASCOM_DISPLAY_X_OFFSET*2))
//    480*NASCOM_DISPLAYSCALEX     // how many pixels the screen is wide  - 48 characters * 8 =  384 + ( 48 offset * 2 ) = 480

#define NASCOM_DISPLAY_HEIGHT  ((NASCOM_DISPLAYLINES*NASCOM_FONT_H*NASCOM_DISPLAYSCALEY)+(NASCOM_DISPLAY_Y_OFFSET*2))
// 256*NASCOM_DISPLAYSCALEY     // how many pixels the screen is high  - 16 line by 15 high = 240 + ( 8 offset *2 ) = 256




extern int nascom_create_screen(BYTE *ScreenMemory);
extern void nascom_display_refresh(void);
extern void nascom_display_change_size(int sizefactor);
extern void nascom_display_position(int x, int y);
extern void nascom_GetWindowSize(int* w, int* h);

// font to use for the display
// see font.c for the NASCOM 2 font from it's character rom
extern uint8_t nascom_font_raw[];

// position of screen on computer screen
extern int nascomdisplayxpos;
extern int nascomdisplayypos;

// to show or hide the nascom window
extern void nascom_show_window();
extern void nascom_hide_window();

// end of file



