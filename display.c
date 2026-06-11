/*
 *   module to define screen and handle keyboard
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "options.h"  //defines the options to map80ram
#include "simz80.h"
#include "map80nascom.h"
#include "map80ram.h"
#include "display.h"
#include "statusdisplay.h"

static SDL_Window *screen;
static SDL_Renderer *rend;
static SDL_Texture *texture;
static uint32_t pixmap[NASCOM_DISPLAY_HEIGHT * NASCOM_DISPLAY_WIDTH];

static BYTE *screenRam;

static void RenderItem(int idx, int xp, int yp);

int nascomdisplayxpos=NASCOM_DISPLAY_XPOS;
int nascomdisplayypos=NASCOM_DISPLAY_YPOS;

int nascom_create_screen(BYTE *ScreenMemory)
{
    // save where screen is stored.
    screenRam=ScreenMemory;

    //printf("Screen memory %p \n",screenRam);

    // see sdlevents.c
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        return 1;
    }

    // to be called when the programs exits
    // moved to sdlevents
    atexit(SDL_Quit);


    screen = SDL_CreateWindow("Nascom 2",
		NASCOM_DISPLAY_XPOS, //SDL_WINDOWPOS_UNDEFINED,
	 	NASCOM_DISPLAY_YPOS, //  SDL_WINDOWPOS_UNDEFINED,
		NASCOM_DISPLAY_WIDTH,
        NASCOM_DISPLAY_HEIGHT,
		SDL_WINDOW_RESIZABLE); // SDL_WINDOW_RESIZABLE);

    if (screen == NULL) {
        fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
        return 1;
    }

    rend = SDL_CreateRenderer(screen, -1, 0);
    if (rend == NULL) {
	    fprintf(stderr, "Unable to create renderer: %s\n", SDL_GetError());
	    return 1;
    }

    if (texture)
	SDL_DestroyTexture(texture);

    texture = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888,
	SDL_TEXTUREACCESS_STREAMING, NASCOM_DISPLAY_WIDTH, NASCOM_DISPLAY_HEIGHT);
    if (texture == NULL) {
	    fprintf(stderr, "Unable to create display texture: %s\n", SDL_GetError());
	    return 1;
    }

    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
    SDL_RenderClear(rend);
    SDL_RenderPresent(rend);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(rend, NASCOM_DISPLAY_WIDTH, NASCOM_DISPLAY_HEIGHT);



    return 0;
}

// will rescale the current nascom window by x factor 

void nascom_display_change_size(int sizefactor){

    // since we build the display at 2x actual pixel size 
    // scaling down if actually asking for scaling of 1

    SDL_SetWindowSize(screen, NASCOM_DISPLAY_WIDTH*sizefactor*.5, NASCOM_DISPLAY_HEIGHT*sizefactor*.5);
    
}

// set the position for nascom display
void nascom_display_position(int x, int y){

    SDL_SetWindowPosition(screen, x, y);

}


void nascom_show_window(){
    // show the nascom window
    SDL_ShowWindow(screen);
    // ensure it has focus
    SDL_SetWindowInputFocus(screen);

}

void nascom_hide_window(){
    // hide the nascom window
    SDL_HideWindow(screen);

}


// get the current size of the nascom window on the screen
// updates the integers pointed to by w and h
void nascom_GetWindowSize(int* w, int* h){

    SDL_GetWindowSize(screen,w,h);
    if (w==NULL){
        *w=NASCOM_DISPLAY_WIDTH;
    }
    if (h==NULL){
        *h=NASCOM_DISPLAY_HEIGHT;
    }
}



/* Would be better to do the updating on demand and the push here */
// fix DA - need to ensure we pick up the RAM entry not ram
//     that is allow for the paging process.
// this is called when the simz80 code calls the sim_delay function

void nascom_display_refresh(void)
{
    static uint8_t screencache[1024] = { 0 };
    bool dirty = false;


    // every x cycles refresh the display
    static int countdown = NASCOM_DISPLAYFORCEREFRESH;
    if ((countdown--)<1){
        countdown = NASCOM_DISPLAYFORCEREFRESH;
        dirty=true;
    }

/*
    for (uint8_t *p0 = ram + 0x80A, *q0 = screencache + 0xA;
         p0 < ram + 0xC00; p0 += 64, q0 += 64)
        for (unsigned char *p = p0, *q = q0; p < p0 + 48; ++p, ++q)
            if (*q != *p) {
                *q = *p;
                unsigned index = p - ram - 0x800;
                unsigned x     = index % 64 - 10;
                unsigned y     = index / 64;
                y = (y + 1) % 16; // The last line is the first line

                RenderItem(*p, x * NASCOM_FONT_W, y * NASCOM_FONT_H);
                dirty = true;
            }
*/
    // fix DA changed this as got confused with the rampagetable and pointers
    //  This seems to do the same thing as the previous code.
    //  with the difference that it used RAM to get to the screen memory
    //  that is in a different block of memory to ram

    uint8_t *screencacheAddress = screencache + 0xA;
    // for each line on the screen
    // normally screen is 80A to BFF
    // as special ram for screen it is now 00A 4FF
    for (WORD screenAddress = 0x00A ;
               screenAddress <  0x400; screenAddress += 64, screencacheAddress += 64) {

        uint8_t *cacheByte = screencacheAddress;  // address into screen cache to use in the next loop

        // for each character in the line
        for (WORD screenAddress1 = screenAddress;
               screenAddress1 < screenAddress + 48; ++screenAddress1, ++cacheByte) {

            BYTE screenByte = (screenRam[screenAddress1]);

            if (*cacheByte != screenByte) {
                *cacheByte = screenByte;
                unsigned index = screenAddress1;
                unsigned x     = index % 64 - 10;
                unsigned y     = index / 64;
                y = (y + 1) % 16; // The last line is the first line

                // printf("Screen Address [%02X] X [%02X] Y [%02X] \n",index,x,y);
                // calculate the actual pixel positions on the screen
                // moved the calculate to the RenderItem function
                // RenderItem(*q, x * NASCOM_FONT_W, y * NASCOM_FONT_H);
                RenderItem(*cacheByte, x, y);
                dirty = true;
            }
        }
    }

    if (dirty) {
        // update the display
        //printf("updating nascom screen \n");
	    SDL_Rect sr;
	    sr.x = 0;
	    sr.y = 0;
	    sr.w = NASCOM_DISPLAY_WIDTH;
	    sr.h = NASCOM_DISPLAY_HEIGHT;
	    SDL_UpdateTexture(texture, NULL, pixmap, NASCOM_DISPLAY_WIDTH * 4);
	    SDL_RenderClear(rend);
	    SDL_RenderCopy(rend, texture, NULL, &sr);
	    SDL_RenderPresent(rend);
    }
}



// render one character on the screen using the Nascom character font
// loaded in font.c
static void RenderItem(int idx, int xp, int yp)
{
    // calculate the offset into the "font" array
    // each character is 16 bytes - added () to show how calulation works
	uint8_t *p = nascom_font_raw + (NASCOM_BYTESPERCHARACTER * idx);
	// calculate where on the bitmap to start the character
	// The offsets add margins on the screen
// x * NASCOM_FONT_W, y * NASCOM_FONT_H
	//
	uint32_t *r = pixmap + (NASCOM_DISPLAY_X_OFFSET) +   // step in to start of line
	        (NASCOM_DISPLAY_Y_OFFSET * NASCOM_DISPLAY_WIDTH ) + // step down to start line
	        /* Only this bit is actually not part of a constant */
	        // updated include width and height here and add brackets
	        (xp * NASCOM_FONT_W * NASCOM_DISPLAYSCALEX ) +     // how far across the line we are
	        (yp * NASCOM_FONT_H * NASCOM_DISPLAY_WIDTH * NASCOM_DISPLAYSCALEY);     // how many lines down we are

	int8_t y, x;

	for (y = 0; y < NASCOM_FONT_H; y++) {
        // check if we need to do more than 1 line if scaling
        uint8_t c = *p;  // font line to use 
        for (int scaleyetc = 0 ; scaleyetc < NASCOM_DISPLAYSCALEY ; scaleyetc++ ){
            for (x = NASCOM_FONT_W - 1; x >= 0; x--){
                // check if we need to do more than 1 if scaling
                for (int scalexetc = 0 ; scalexetc < NASCOM_DISPLAYSCALEX ; scalexetc++ ){
                    // not sure why lots of Fs but
                    // it's a colour  0xFFFF0000 is red
                    *r++ = (c & (1 << x)) ? STATUS_GREEN : 0;
                }
            }
            // next line we have moved on 8 bytes on the display depending upon the scale
            r += NASCOM_DISPLAY_WIDTH - (NASCOM_FONT_W * NASCOM_DISPLAYSCALEX);
        }
        p++;   // next font line
	}
}





// end of file



