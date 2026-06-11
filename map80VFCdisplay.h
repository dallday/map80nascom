/*
 * definitions for the map80VFCdisplay.c
 *
 */

#define MAP80VFCDISPLAYSCALEY       2
#define MAP80VFCDISPLAYSCALEX       1
#define MAP80VFCDISPLAYCHARACTERS  80
#define MAP80VFCDISPLAYLINES       25

#define MAP80VFCDISPLAY_BYTESPERCHARACTER 16     // number of bytes in each character from the font file

// #define MAP80VFCDISPLAY_FONT_H       16			// number of pixels high 16
//#define MAP80VFCDISPLAY_FONT_H       10			// 
// actually only using 10 lines
#define MAP80VFCDISPLAY_FONT_H       10		// number of pixels high 16
// actually 10 lines doubled

#define MAP80VFCDISPLAY_FONT_W        8         // number of pixels wide

#define MAP80VFCDISPLAY_DISPLAY_X_OFFSET  24     // number of pixels in left and right border
#define MAP80VFCDISPLAY_DISPLAY_Y_OFFSET   8     // number of pixels in top and bottom border

// set sizes for the display window
#define MAP80VFCDISPLAY_DISPLAY_WIDTH  ((MAP80VFCDISPLAYCHARACTERS * MAP80VFCDISPLAY_FONT_W * MAP80VFCDISPLAYSCALEX ) + ( MAP80VFCDISPLAY_DISPLAY_X_OFFSET * 2 )) 
//   how many pixels the screen is wide  - 80 characters * 8 =  384 + ( 48 offset * 2 ) = 480
#define MAP80VFCDISPLAY_DISPLAY_HEIGHT  ((MAP80VFCDISPLAYLINES * MAP80VFCDISPLAY_FONT_H * MAP80VFCDISPLAYSCALEY ) +  ( MAP80VFCDISPLAY_DISPLAY_Y_OFFSET * 2 )) 
//  how many pixels the screen is high  - 25 line by 16 high = 240 + ( 8 offset *2 ) = 256

/*
 * Indexes to the registers in the 6845 CRT Controller
 */

#define MAP80_6845_HORIZONTAL_TOTAL      (0)
//    This is the Horizontal frequency - not needed for emulation

#define MAP80_6845_HORIZONTAL_DISPLAYED  (1)
//    This is the number of characters displayed per line on the screen
// Note:- this impacts on the number of bytes used per line
//

#define MAP80_6845_H_SYNC_POSITION       (2)
//     This is the horizontal sync position on the horizontal line
//      not needed for emulation
#define MAP80_6845_H_SYNC_WIDTH            (3)
//     This is the horizontal sync (HS) pulse width
//      not needed for emulation

#define MAP80_6845_VERTICAL_TOTAL        (4)
#define MAP80_6845_VERTICAL_ADJUST       (5)
//      These 2 fileds are used to determine the Vertical Sync (VS) pulse

#define MAP80_6845_VERTICAL_DISPLAYED    (6)
//      Defines number of vertical lines displayed


#define MAP80_6845_VERTICAL_SYNC_POSITION       (7)
//      This is used to determine the vertical sync (VS) position

#define MAP80_6845_INTERLACE_MODE        (8)
//   2 bits to determine if interlace mode is selected
//   00 means no interlacing
//   not sure about the other 2 settings

#define MAP80_6845_MAX_SCANLINE          (9)
//   This defines the font height in lines -1

#define MAP80_6845_CURSOR_START          (10)
//  This defines if the cursor blinks and the cursor start line in character.
//        bit 6 enables blink
//        bit 5 blink frequency
//          bit 6 5
//              0 0 non-blink
//              0 1 no cursor
//          bit 0-4 cursor start line in character

#define MAP80_6845_CURSOR_END            (11)
//          bit 0-4 cursor end line in character

#define MAP80_6845_START_ADDRESS_H               (12)
// the 6 high bits of the address
#define MAP80_6845_START_ADDRESS_L               (13)
// the 8 low bits of the address
// together they define where in memory the screne memory is
// The MAP80 VFC has 4k memory
//  ?000 to ?7FF is used by the ROM
//  ?800 to ?FFF is ram that can be used for the screen
//  The actual chip could use 16k of memory but not in the MAP80 VFC card.


#define MAP80_6845_CURSOR_H              (14)
#define MAP80_6845_CURSOR_L              (15)
//     cursor position - 6 bit higher and 8 bit lower
//     Where the cursor is the memory within the 32k block it could address
//         but in this case it's within the 4k for map80 VFC
//

#define MAP80_6845_LIGHTPEN_H            (16)
#define MAP80_6845_LIGHTPEN_L            (17)
//        light pen position - 6 bit higher and 8 bit lower
//        defines at what address the LPSTB input pulses high
//

#define MAP80_6845_NUMBEROFREGISTERS     (18)
// used to ensure we don't output off the table where the register values are stored.

// the byte sent to port 0xEC or 0xED
// the bits used by the MAP80VFC card to define where the memory is loaded
#define MAP80VFC_4KBITS (0xF0)      // 4k boundary set by top 4 bits
#define MAP80VFC_RAMENABLE (0x01)   // brings in display ram to the lower 2k of the 4k area (1) - or not (0)
#define MAP80VFC_ROMENABLE (0x02)   // brings in ROM to the upper 2k of the 4k area (1) - or not (0)
                                    // but this is reversed if the cpmswitch variable = 1
#define MAP80VFC_INVERSE_VIDEO  (0x04) 
#define MAP80VFC_CHARGEN_SELECT (0x08)

#define CURSORCOUNTDOWNSTARTVALUE 1

extern int vfcdisplaydebug;

// defines where these are on the main screen
extern int map80vfcdisplayxpos;
extern int map80vfcdisplayypos;


extern int map80vfc_create_screen(BYTE *screenMemory);    // creates the screen
extern void map80vfc_display_refresh(void);        // refresh the screen from memory
extern void map80vfc_display_change_size(int sizefactor);
extern void map80vfc_display_position(int x, int y);
// get the current size of the nascom window on the screen
// updates the integers pointed to by w and h
extern void map80vfc_GetWindowSize(int* w, int* h);

extern void VFC_show_window();
extern void VFC_hide_window();

// handle the ports for the vfc video card
extern int inPortVFCDisplay(unsigned int port);
extern void outPortVFCDisplay(unsigned int port, unsigned int value);
