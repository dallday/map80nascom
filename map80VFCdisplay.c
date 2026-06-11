/*
 *   module to define VFC screen stuff
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "options.h"  //defines the options to map80nascom
#include "simz80.h"
#include "map80nascom.h"
#include "map80ram.h"
#include "map80VFCcharRom1.h"
#include "map80VFCdisplay.h"
#include "cpmswitch.h"
#include "statusdisplay.h"

#include "display.h"


/* Initialises data */

// display debug messages
//int vfcdisplaydebug=VFCDISPLAYDEBUG;

// defines where these are on the main screen
int map80vfcdisplayxpos=MAP80VFC_DISPLAY_XPOS;
int map80vfcdisplayypos=MAP80VFC_DISPLAY_YPOS;



static SDL_Window *screen=NULL;
static SDL_Renderer *rend=NULL;
static SDL_Texture *texture=NULL;
static uint32_t pixmap[MAP80VFCDISPLAY_DISPLAY_HEIGHT * MAP80VFCDISPLAY_DISPLAY_WIDTH];

// a pointer to the memory where the screen characters are stored.
static BYTE *screenRam=NULL;

// TODO need to look at how these values impact on the display 
// and how many we are going to just ingore 
// for now assuming it is 80 x 25 and font is 10 lines
// These are the 6845 display chip registers 
// Values set by VFC initialise routine as it starts up 
// High/low addresses address position inside the 16k block that the graphic chip could address.
//   actual address is done by ((highaddress << 8) + lowaddress) & 0x3FFF
//  but the VFC card only has 4k of memory ( 2k rom and 2k ram ) 
//    with the video ram starting at 0x800 - so suggest 
//   actual address is done by ((highaddress << 8) + lowaddress) & 0x0FFF
//    and maybe use & 0x07FF to remap it to 0 to 7FF.
//  
static unsigned char map80_6845_registers[] = 
{  0x72,                                          // 00 Horizontal total chars - 1  Not used */
   MAP80VFCDISPLAYCHARACTERS,                          // 01 Horizontal displayed characters */
   0x5a,                                          // 02 Horizontal sync position -1 not used */
   0x67,                                          // 03 Vertical/Horizontal sync not used  */
   0x1d,                                          // 04 Vertical character lines -1 */
   0x0b,                                          // 05 Vertical scan lines adjust */
   MAP80VFCDISPLAYLINES,                          // 06 Vertical displayed lines in number of characters*/
   0x1b,                                          // 07 Vertical sync position - not used */
   0x00,                                          // 08 Interlace and skew - not used */
   MAP80VFCDISPLAY_FONT_H-1,                      // 09 Characters per line -1 */
   0x48,                                          // 0A Cursor type  and start raster */
   0x09,                                          // 0B Cursor end raster */
   0x08,                                          // 0C Video start in memory space  - high 
   0x00,                                          // 0D and low  ( it's at 0x800 in the map80vfc address space)
   0x0f,                                          // 0E where the cursor is  high 
   0xff,                                          // 0F and low 
   0x00,                                          // 10 light gun address high 
   0x00                                           // 11    and low 
};

// set by call to 0xEA - says which register is being looked at
static int map80_6845_registerPointer =0;
// decode the cursor type using the 
//cursor control Cursor start register 
//Bit 6 5 
//    0 0 non-blink
//    0 1 cursor non-display
//    1 0 blink 1/16 rate
//    1 1 blink 1/32 rate
//bits 0 to 4 start row in character
//    0 to 0x1F positions 
// cursor control cursor end register 
//  bits 0 to 4 give end row  
//    0 to 0x1F positions 
static int cursorBlinking = 1; 
// use when the blick speed can be changed
//static int cursorBlinkSpeed = 0; // slow blink
static int cursorStartrow = 8;
static int cursorEndrow = 9;

static int vfcdisplayinversevideo=0; // 0 means use upper 128 character set else use inverse video

#define MAP80_6845_CURSORBLINKMASK 0x20
#define MAP80_6845_CURSORSPEEDMASK 0x40
#define MAP80_6845_CURSORSTARTMASK 0x1F
#define MAP80_6845_CURSORENDMASK   0x1F


// VideoControlPort swaps the memory in and out of main ram
static void processVideoControlPort(unsigned int value);


int map80vfc_create_screen(BYTE *screenMemory){

    // screen memory 
    screenRam=screenMemory;


    /*
    * Initialises the SDL video subsystem (as well as the events subsystem).
    * Returns 0 on success or a negative error code on failure using SDL_GetError().
    */
    /* see sdlevents.c
        // done in the nascom display ?? but seems happy to be called twice ?
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL failed to initialise: %s\n", SDL_GetError());
            return 1;
    }
    */

    /* Creates a SDL window */

    screen = SDL_CreateWindow("MAP80VFC Display", // Title of the SDL window 
                map80vfcdisplayxpos, //  Position x of the window 
                map80vfcdisplayypos, //  Position y of the window 
                MAP80VFCDISPLAY_DISPLAY_WIDTH, // Width of the window in pixels 
                MAP80VFCDISPLAY_DISPLAY_HEIGHT, // Height of the window in pixels 
                SDL_WINDOW_RESIZABLE);  // SDL_WINDOW_RESIZABLE // Additional flag(s)

    printf("display width %d display height %d \n",MAP80VFCDISPLAY_DISPLAY_WIDTH,MAP80VFCDISPLAY_DISPLAY_HEIGHT);

    /* Checks if window has been created; if not, exits program */

    if (screen == NULL) {
        fprintf(stderr, "SDL VFC window failed to initialise: %s\n", SDL_GetError());
        return 1;
    }

    rend = SDL_CreateRenderer(screen, -1, 0);
    if (rend == NULL) {
        fprintf(stderr, "Unable to create renderer: %s\n", SDL_GetError());
        return 1;
    }

    if (texture)
    SDL_DestroyTexture(texture);

    texture = SDL_CreateTexture(rend,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                MAP80VFCDISPLAY_DISPLAY_WIDTH,
                                MAP80VFCDISPLAY_DISPLAY_HEIGHT);
    if (texture == NULL) {
        fprintf(stderr, "Unable to create display texture: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
    SDL_RenderClear(rend);
    SDL_RenderPresent(rend);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(rend, MAP80VFCDISPLAY_DISPLAY_WIDTH, MAP80VFCDISPLAY_DISPLAY_HEIGHT);

    return 0;

}

void map80vfc_display_change_size(int sizefactor){
    
    // since we build the display at 2x actual pixel size 
    // scaling down if actually asking for scaling of 1

    SDL_SetWindowSize(screen, MAP80VFCDISPLAY_DISPLAY_WIDTH*sizefactor*.5, MAP80VFCDISPLAY_DISPLAY_HEIGHT*sizefactor*.5);
    
}

void map80vfc_display_position(int x, int y){

    SDL_SetWindowPosition(screen, x, y);

}

void VFC_show_window(){
    // show the nascom window
    SDL_ShowWindow(screen);
    // ensure it has focus
    SDL_SetWindowInputFocus(screen);
}

void VFC_hide_window(){
    // hide the nascom window
    SDL_HideWindow(screen);

}


// get the current size of the nascom window on the screen
// updates the integers pointed to by w and h
void map80vfc_GetWindowSize(int* w, int* h){

    SDL_GetWindowSize(screen,w,h);
    if (w==NULL){
        *w=MAP80VFCDISPLAY_DISPLAY_WIDTH;
    }
    if (h==NULL){
        *h=MAP80VFCDISPLAY_DISPLAY_HEIGHT;
    }
}




/* Would be better to do the updating on demand and the push here */
// fix DA - need to ensure we pick up the RAM entry not ram
//     that is allow for the paging process.
// this is called when the simz80 code calls the sim_delay function


void map80vfc_display_refresh(void)
{
    // hold a copy of the screen memory to compare and see what has changed
    static uint8_t screencache[2*1024] = { 0 };
    bool dirty = false;

    // timing between cursor flashing
    static int cursorcountdown=CURSORCOUNTDOWNSTARTVALUE;
    // hold last cursor address - used to reset where the cursor was 
    static WORD lastcursorAddress=0;

    // every x cycles force a refresh the display
    static int countdown = MAP80VFC_DISPLAYFORCEREFRESH;
    if ((countdown--)<1){
        countdown = MAP80VFC_DISPLAYFORCEREFRESH;
        dirty=true;
    }

    // set where the cursor is 
    // use the 6 bits from high address byte and all 8 bits from the low address byte
    // which gives the address of the byte in the screen memory.
    // but since we really only have 2k of screen memory on the map80 vfc 
    //  & high address with 0x07 using only 3 bits from top address
    WORD cursorAddress = ((map80_6845_registers[MAP80_6845_CURSOR_H] << 8) + map80_6845_registers[MAP80_6845_CURSOR_L]) & 0x7FF;
    // printf("Cursor is at [%4.4X]\n",cursorAddress);
    //printf("High address [%2.2X], shifted [%2.2X]\n",map80_6845_registers[MAP80_6845_CURSOR_H],map80_6845_registers[MAP80_6845_CURSOR_H]<<8);

    // set start address within the memory space 
    // interesting effect if we go outside of the memory space 
    //WORD startAddress = ((map80_6845_registers[MAP80_6845_START_ADDRESS_H] << 8) + map80_6845_registers[MAP80_6845_START_ADDRESS_H]) & 0x7FF;
    // TODO think how this could be implimented


    // uses the screen ram to set the characters 
    // this may or maynot be mapped into the z80 rampage table
    // but this code does not care
    
    // the cache is a copy of the current screen and is used to see if anything has changed
    uint8_t *screencacheAddress = screencache ;
    
    // for each line on the screen
    // no skip bits in the vfc ram
    // as special ram for screen it is now 00A 4FF
    WORD screenAddress=0; //used in loops set here so I could check after the loops
    WORD screenAddress1=0; // - - - - ditto - - - - -
    int xpos=0; // characters in pixels on line 
    int ypos=0; // characters in pixels down screen
    int cursorshow=0; // set to 1 to show cursor in a character on screen
    int updateBitmap=0; // refresh the current screen character in the bitmap
    static int cursoron=0; // set to 1 if cursor was displayed last cycle
    uint8_t *fontAddress=NULL;
    //printf("cursor address %4.4X last one %4.4X\n",cursorAddress,lastcursorAddress);

    if (cursorAddress != lastcursorAddress){
        cursoron=0;  // say we have not displayed cursor at new address
        cursorcountdown=0; // and need to start blinking again
    }

    // cycle through the screen memory 
    // it is possible for the 6845 chip to not start the display at 0 but , , , , , TODO
    // MAP80_6845_START_ADDRESS_H and L
    for (screenAddress = 0x00 ;
               screenAddress <  0x7D0; 
               screenAddress += MAP80VFCDISPLAYCHARACTERS, screencacheAddress += MAP80VFCDISPLAYCHARACTERS, 
               ypos+=(MAP80VFCDISPLAY_FONT_H*MAP80VFCDISPLAY_DISPLAY_WIDTH*MAP80VFCDISPLAYSCALEY))  // actual pixel position pixel map
        {

        uint8_t *cacheByte = screencacheAddress;  // address into screen cache to use in the lower loop
        xpos=0;           // start of new line

        // for each character in the line - (80) MAP80VFCDISPLAYCHARACTERS characters
        for (screenAddress1 = screenAddress;
               screenAddress1 < screenAddress + MAP80VFCDISPLAYCHARACTERS; ++screenAddress1, ++cacheByte, 
               xpos+=(MAP80VFCDISPLAY_FONT_W*MAP80VFCDISPLAYSCALEX)) 
            {
            
            // current character in screen ram
            BYTE screenByte = (screenRam[screenAddress1]);

            // need to check for cursor 
            updateBitmap=0;     // set we don't need to update bitmap
            cursorshow=0;       // set we don't need to show cursor
            
            if (screenAddress1==cursorAddress){
                
                //printf("cursorhere address %4.4X cursor show %d\n",cursorAddress,cursorshow);
                // this is where the cursor should be
                if (cursorBlinking==1){
                    // check if we need to change cursor 
                    if (cursorcountdown-- < 1){
                        // time to change cursor
                        cursorcountdown=CURSORCOUNTDOWNSTARTVALUE;
                        // either turn it on or off
                        if (cursoron==1){
                            // cursor off
                            cursoron=0;     // cursor is now off
                            updateBitmap=1; // this is the cursor position and bitmap needs to be reset
                        }
                        else {
                            cursoron=1;     // cursor is now on
                            cursorshow=1;  // say we need to show cursor
                            updateBitmap=1; // and refresh this character on the cache
                        }
                    }
                }
                else {
                    // static cursor - check if already displayed
                    if ( cursoron == 0){
                        updateBitmap=1; // turn cursor on
                        cursoron=1; // put it on now 
                        cursorshow=1; // and show it 
                        //printf("static cursor address %4.4X cursor show %d\n",cursorAddress,cursorshow);
                    }
                }

            }
            // check if we need to reset the bitmap used for the old cursor
            if (lastcursorAddress!=cursorAddress ){
                if (screenAddress1==lastcursorAddress ) {
                    updateBitmap=1; // don't worry if old cursor was on or off just reset it
                }
            
            }
            // render the character if needed
            if (*cacheByte != screenByte ||  updateBitmap==1 ) {
                // need to update screen cache and bitmap pixel
                // save new value
                *cacheByte = screenByte;
                // get the address of the first line of the font 
                        // if inverse video and top 128 characters - use lower 128 character
                if (( vfcdisplayinversevideo!=0) &&  (screenByte>0x7F)) {
                    fontAddress = map80VFCcharRom1 + (MAP80VFCDISPLAY_BYTESPERCHARACTER * (screenByte-128));
                }
                else {
                    fontAddress = map80VFCcharRom1 + (MAP80VFCDISPLAY_BYTESPERCHARACTER * screenByte);
                }
                // get where to start in the pixmap
                // addressofpixmap + x pixels offset ( y pixels offset * width in pixels ) 
                //    xpos and ypos are already in pixels 
                uint32_t *pixmapAddress = pixmap + MAP80VFCDISPLAY_DISPLAY_X_OFFSET +
                                    ( MAP80VFCDISPLAY_DISPLAY_Y_OFFSET * MAP80VFCDISPLAY_DISPLAY_WIDTH ) +
                                    ( xpos ) + (ypos);

                // now process the lines of the font
                for (int y = 0; y < MAP80VFCDISPLAY_FONT_H; y++) {
                    // doing 1 row of the characters pixels
                    uint8_t fontLine = *fontAddress;
                    if (vfcdisplayinversevideo!=0){
                        // if inverse video and top 128 characters - inverse it
                        if (screenByte>0x7F){
                            fontLine = ~fontLine;
                        }
                    }
                    // check if we are at cursor position and if we need to set it
                    if (cursorshow==1 && ( y >= cursorStartrow && y <= cursorEndrow)){
                        // invert line for cursor
                        fontLine ^= 0xFF; 
                    }
                    // check if we need to do more than 1 line per font line i.e. if scaling
                    for (int scaleyetc = 0 ; scaleyetc < MAP80VFCDISPLAYSCALEY ; scaleyetc++ ){
                        // now do one line of 8 pixels
                        for (int x = MAP80VFCDISPLAY_FONT_W - 1; x >= 0; x--){
                            // doing a pixel
                            // check if we need to do more than 1 pixel - i.e. if scaling
                            for (int scalexetc = 0 ; scalexetc < MAP80VFCDISPLAYSCALEX ; scalexetc++ ){
                                //printf("pixel x %2.2X y %2.2X \n",x,y);
                                *pixmapAddress++ = (fontLine & (1 << x)) ? STATUS_GREEN : 0;
                            }
                        }
                        pixmapAddress += MAP80VFCDISPLAY_DISPLAY_WIDTH - (MAP80VFCDISPLAY_FONT_W*MAP80VFCDISPLAYSCALEX); 
                        // minus MAP80VFCDISPLAY_FONT_W (8) as just done 8 bits * scaling !
                    }
                    fontAddress++;
                }
                
                dirty = true;   // set marker to tell the code to redisplay
            }
        }
    }

    lastcursorAddress=cursorAddress;

    //printf("max screen address [%4.4X] \n",screenAddress1);

    if (dirty) {
        SDL_Rect sr;
        sr.x = 0;
        sr.y = 0;
        sr.w = MAP80VFCDISPLAY_DISPLAY_WIDTH;
        sr.h = MAP80VFCDISPLAY_DISPLAY_HEIGHT;
        // think 4 bytes per pixel 
        SDL_UpdateTexture(texture, NULL, pixmap, MAP80VFCDISPLAY_DISPLAY_WIDTH * 4 );
        // remove current picture
        SDL_RenderClear(rend);
        // create a new picture to display 
        SDL_RenderCopy(rend, texture, NULL, &sr);
        // display it - replace current frame with new data
        SDL_RenderPresent(rend);
    }
}





// handle all calls to the MAP80 VFC output ports
void outPortVFCDisplay(unsigned int port, unsigned int value){

    switch (port) {
    case 0xE6:
    case 0xE7:
        // read only - read keyboard
        break;
    case 0xE8:
    case 0xE9:
        // E8 or E9 pulse alarm
        break;
    case 0xEA:
        // write only 6845 chip register select
        if (vfcdisplaydebug){
            printf("setting 6845 address [%2.2X]\n",value);
        }
        map80_6845_registerPointer = value;
        break;
    case 0xEB:
        // write to 6845 chip register set by EA
        if (vfcdisplaydebug){
            printf("setting 6845 address [%2.2X] to [%2.2X]\n",map80_6845_registerPointer,value);
        }
        if ( map80_6845_registerPointer < MAP80_6845_NUMBEROFREGISTERS){
            map80_6845_registers[map80_6845_registerPointer]=value;
        }
        else{
            printf("invalid address [%2.2X]\n",MAP80_6845_NUMBEROFREGISTERS);
        }
        break;
    case 0xEC:
    case 0xED:
        // EC and ED write only video control ports
        processVideoControlPort(value);
        break;
    case 0xEE:
        // select video 1
        nascom_hide_window();
        VFC_show_window();
        break;
    case 0xEF:
        // select video 2
        VFC_hide_window();
        nascom_show_window();
        break;

    default:
        fprintf(stderr,"MAP80 VFC Display unhandled write to port [%2X] value [%2X] \n",port,value);
        break;
    }

}


int inPortVFCDisplay(unsigned int port){

    switch (port) {
    case 0xE6:
    case 0xE7:
        // E6 and E7 read keyboard
        return 0;
        break;
    case 0xE8:
    case 0xE9:
        // E8 or E9 pulse alarm
        return 0;
        break;
    case 0xEA:
        // write only 6845 chip register select
        if (vfcdisplaydebug){
            printf("Reading setting 6845 address [%2.2X]\n",map80_6845_registerPointer);
        }
        return map80_6845_registerPointer;
        break;
    case 0xEB:
        // read 6845 chip register set by EA
        // write to 6845 chip register set by EA
        if ( map80_6845_registerPointer < MAP80_6845_NUMBEROFREGISTERS){
            if (vfcdisplaydebug){
                printf("Reading 6845 address [%2.2X] value [%2.2X]\n",map80_6845_registerPointer,map80_6845_registers[map80_6845_registerPointer]);
            }
            return map80_6845_registers[map80_6845_registerPointer];

        }
        else{
            printf("Reading 6845 address [%2.2X] failed - invalid address\n",MAP80_6845_NUMBEROFREGISTERS);
        }
        break;
    case 0xEC:
    case 0xED:
        // EC and ED write only video control ports
        return 0;
        break;
    case 0xEE:
        // select video 1
        nascom_hide_window();
        VFC_show_window();
        return 0;
        break;
    case 0xEF:
        // select video 2
        VFC_hide_window();
        nascom_show_window();
        return 0;
        break;
    default:

        fprintf(stderr,"MAP80 VFC Display unhandled read on port [%2X] \n",port);
    }

    return 0xFF;


}


// process the video control port 
// DA N4 note we are now using 1k pages not 2k pages 

void processVideoControlPort(unsigned int value){

    // check if inverse video or top 128 character set
    
    vfcdisplayinversevideo = value & MAP80VFC_INVERSE_VIDEO;

    // allowing ram and rom to be enabled at various addresses

    static int vfcRomEntry=-1; // -1 means not allocated
    static int vfcDisplayEntry=-1; // -1 means not allocated
    //static int vfc4kEntry=0; // default value should be reset by first call

    int newvfcRomEntry=-1;
    int newvfcDisplayEntry=-1;
    int newvfcAddress;
    int newvfc4kEntry;

    int enablerom;

    // sort out new 4k boundary
    // actually for rampagetable we need 2k boundaries
    // so shift out 3 bits
    // DA N$ now 1k stuff so need 4 TODO Check
    newvfcAddress = (value & MAP80VFC_4KBITS)<<8; // make the 4k value a full address
    newvfc4kEntry = (newvfcAddress)>>RAMPAGESHIFTBITS; // now work oput start entry

    //printf("port value %2.2X 4kbits %2.2X rampagetable entry %2.2X 4kpage %4.4X\n", value,(value & MAP80VFC_4KBITS),newvfc4kEntry,newvfc4kEntry*RAMPAGESIZE*1024);

    // this is the "link 4" setting 
    // depends on this to say if 0 enables or disables the rom 
    enablerom=value & MAP80VFC_ROMENABLE;

    // check if cpm switch set
    // equivalent of setting the Link 4 on the VFC board
//    if (cpmswitchstate==1){
    // DA N4 now reliant on the bit5 of Port_REMAP ( & 0x20 )
    if (Port_REMAP_value & 0x20){
        // toggle the ROM enable bitPort_REMAP
        enablerom ^= MAP80VFC_ROMENABLE;
        // printf("enable rom toggle %02X \n",enablerom);
    }

    // set to not in memory
    newvfcRomEntry=-1;
    newvfcDisplayEntry=-1;

    if (enablerom){
        newvfcRomEntry=newvfc4kEntry;
    }

    if (value & MAP80VFC_RAMENABLE){
        newvfcDisplayEntry=newvfc4kEntry+2; // add 2 as now stored as 1k areas
    }

    // rom entry
    if (newvfcRomEntry!=vfcRomEntry){
        // need to change rom stuff
        if (vfcRomEntry > -1){
            for (int entrynum=0;entrynum <2;entrynum++){
                // remove current entry
                // assumes 2k pages so allow for vfc rom
                ramlocktable[vfcRomEntry+entrynum] = 0;
                // set so the entry is ram again
                // set so only unset the 0x04 bit
                ramromtable[vfcRomEntry+entrynum] &= 0xFB;
                // reset the 2k pointer back to default value - i.e. ram
                rampagetable[vfcRomEntry+entrynum]=ramdefaultpagetable[vfcRomEntry+entrynum];

                //printf("VFC Rom removed from memory entry %02X address reset to %p for 4k boundary %4.4X\n",
                //vfcRomEntry+entrynum,rampagetable[vfcRomEntry+entrynum],(vfcRomEntry+entrynum)*RAMPAGESIZE*1024);
            }
            if (vfcdisplaydebug){
                printf("VFC ROM disabled rampagetable %02X and %02X \n",
                    vfcRomEntry,vfcRomEntry+1);
            }
        }

        vfcRomEntry=newvfcRomEntry;

        if (vfcRomEntry > -1){
            // to protect the Nascom monitor only do this if ramlock not already set ( nascom ram disable line )
            if ((ramlocktable[vfcRomEntry] == 0) && (ramlocktable[vfcRomEntry] == 0) ){
                for (int entrynum=0;entrynum <2;entrynum++){
                    // prom enable
                    // assumes 2k pages so allow for VFC rom
                    // lock the rampage entry so map80ram wont change it ( nascom ram disable line )
                    ramlocktable[vfcRomEntry+entrynum] = 2;
                    // set so the entry is rom and cannot be updated TODO - should this be a masked to set bit 4 only
                    ramromtable[vfcRomEntry+entrynum] |= 0x04;  // DA N4 changed to say hard ROM do not write to default memory 
                    // set the 2k pointer to the VFC Rom - now 1k
                    rampagetable[vfcRomEntry+entrynum]=&vfcrom[0]+(1024*entrynum);
                    // printf("set vfcRomentry rampagetable %02X  address %p \n",vfcRomentry,rampagetable[vfcRomentry]);
//                    printf("VFC ROM added to memory entry %02X to address to %p for 4k boundary %4.4X\n",
//                        vfcRomEntry+entrynum,rampagetable[vfcRomEntry+entrynum],(vfcRomEntry+entrynum)*RAMPAGESIZE*1024);
                }
                if (vfcdisplaydebug){
                    printf("VFC ROM enabled rampagetable %02X and %02X \n",
                        vfcRomEntry,vfcRomEntry+1);
                }
            }
            else {
                fprintf(stderr,"set vfcRomentry not possible as ramlocktable[%02X] already set to %d for 4k boundary %4.4X\n",
                vfcRomEntry,ramlocktable[vfcRomEntry],vfcRomEntry*RAMPAGESIZE*1024);
                vfcRomEntry=-1;
            }
        }
    }
/* *******************************
 * handle display enable . disable
 *********************************
 */


    if (newvfcDisplayEntry!=vfcDisplayEntry){

        if (vfcDisplayEntry > -1){
            // remove current entry
            // reset the 2k pointer back to default value - i.e. ram
            ramlocktable[vfcDisplayEntry] = 0;
            rampagetable[vfcDisplayEntry]=ramdefaultpagetable[vfcDisplayEntry];
            ramlocktable[vfcDisplayEntry+1] = 0;
            rampagetable[vfcDisplayEntry+1]=ramdefaultpagetable[vfcDisplayEntry+1];
            if (vfcdisplaydebug){
                printf("VFC Display disabled rampagetable %02X and %02X \n",
                    vfcDisplayEntry,vfcDisplayEntry+1);
            }

            //printf("VFC Display removed from memory entry %02X address reset to %p for 4k boundary %4.4X\n",vfcDisplayEntry,rampagetable[vfcDisplayEntry],vfcDisplayEntry*RAMPAGESIZE*1024);

        }

        vfcDisplayEntry=newvfcDisplayEntry;

        if (vfcDisplayEntry > -1){

            if ((ramlocktable[vfcDisplayEntry] == 0) && (ramlocktable[vfcDisplayEntry+1] == 0)){
                // assumes 2k pages s
                // lock the rampage entry so map80ram wont change it ( nascom ram disable line )
                ramlocktable[vfcDisplayEntry] = 2;
                // set the 2k pointer to the VFC Rom
                rampagetable[vfcDisplayEntry]=&vfcdisplayram[0];
                //printf("VFC Display added to memory entry %02X to address to %p for 4k boundary %4.4X\n",vfcDisplayEntry,rampagetable[vfcDisplayEntry],vfcDisplayEntry*RAMPAGESIZE*1024);
                ramlocktable[vfcDisplayEntry+1] = 2;
                // set the 2k pointer to the VFC Rom
                rampagetable[vfcDisplayEntry+1]=&vfcdisplayram[0]+(1*1024);
//                printf("VFC Display added to memory entry %02X to address to %p for 4k boundary %4.4X\n",
//                    vfcDisplayEntry+1,rampagetable[vfcDisplayEntry+1],(vfcDisplayEntry+1)*RAMPAGESIZE*1024);
                if (vfcdisplaydebug){
                    printf("VFC Display enabled rampagetable %02X and %02X \n",
                        vfcDisplayEntry,vfcDisplayEntry+1);
                }
            }
            else {
                fprintf(stderr,"set vfcDisplayEntry not possible as ramlocktable[%02X] already set to %d for 4k boundary %4.4X\n",
                    vfcDisplayEntry,ramlocktable[vfcDisplayEntry],vfcDisplayEntry*RAMPAGESIZE*1024);
                vfcDisplayEntry=-1;
            }
        }
    }

}



// end of file

