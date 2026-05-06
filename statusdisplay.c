/*
 *   module to handle status screen stuff
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
#include "statusdisplay.h"
#include "utilities.h"
#include "serial.h"



/* Initialises data */

static SDL_Window *screen=NULL;
static SDL_Renderer *rend=NULL;
static SDL_Texture *texture=NULL;
static uint32_t pixmap[STATUS_DISPLAY_HEIGHT * STATUS_DISPLAY_WIDTH];

// a pointer to the memory where the screen characters are stored.
static BYTE *statusScreenRam=NULL;

static int needsrefresh=0;

int statusdisplayxpos=STATUS_DISPLAY_XPOS;
int statusdisplayypos=STATUS_DISPLAY_YPOS;


void displaytapestatus(void){

    char nofile[]="no file";
    // use this to display if data avialable or not 
    // reposition the pointer at i if data available 
    char nodata[]="no input data available   ";
    char * displaynodata=nodata;
    // buffer to build strings into 
    // da apr 2026 increased size to avoid overflow
    char strBuffer[150];
    // build filename string into 
    char strserialname[100];

    if (serial_input_filename==NULL){
        copystringtobuffer(strserialname,nofile,20);
        //strserialname[0]=0;
    }
    else{
        copystringtobuffer(strserialname,serial_input_filename,20);
        // sprintf(strBuffer,"Serial in: %s %c position:%06ld ",strserialname, " *"[tape_led & 0x1] , tape_in_pos ); 
    }
    sprintf(strBuffer,"Serial in  %06ld file: %s", tape_in_pos, strserialname ); 

    status_display_show_chars_full(strBuffer,0,STATUS_DISPLAYLINES-2,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_LIGHTBLUE,STATUS_BLACK);

    if (serial_output_filename==NULL){
        copystringtobuffer(strserialname,nofile,20);
        //strserialname[0]=0;
    }
    else{
        copystringtobuffer(strserialname,serial_output_filename,20);
        // sprintf(strBuffer,"Serial in: %s %c position:%06ld ",strserialname, " *"[tape_led & 0x1] , tape_in_pos ); 
    }

    sprintf(strBuffer,"Serial out %06ld file: %s", tape_out_pos, strserialname ); 

    status_display_show_chars_full(strBuffer,0,STATUS_DISPLAYLINES-1,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_LIGHTBLUE,STATUS_BLACK);
    
    
    sprintf(strBuffer,"Tape LED "); 
        
    status_display_show_chars_full(strBuffer,0,STATUS_DISPLAYLINES-3,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_LIGHTBLUE,STATUS_BLACK);

    sprintf(strBuffer,"%c","\xB8\xB9"[tape_led & 0x1]); 
        
    status_display_show_chars_full(strBuffer,10,STATUS_DISPLAYLINES-3,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_RED,STATUS_BLACK);
    
    if (serial_input_available){
        displaynodata+=3;
    }
    
    sprintf(strBuffer,"%s",displaynodata); 
        
    status_display_show_chars_full(strBuffer,15,STATUS_DISPLAYLINES-3,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_LIGHTBLUE,STATUS_BLACK);
    //status_display_show_chars_full("SS",10,STATUS_DISPLAYLINES,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_RED,STATUS_BLACK);

}






int status_create_screen(BYTE *screenMemory){

    // screen memory 
    statusScreenRam=screenMemory;


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

    screen = SDL_CreateWindow("Map80Nascom Status", // Title of the SDL window 
                statusdisplayxpos, //  Position x of the window 
                statusdisplayypos, //  Position y of the window 
                STATUS_DISPLAY_WIDTH, // Width of the window in pixels 
                STATUS_DISPLAY_HEIGHT, // Height of the window in pixels 
                SDL_WINDOW_RESIZABLE);   // SDL_WINDOW_RESIZABLE removed stay as is // Additional flag(s)

    // printf("display width %d display height %d \n",STATUS_DISPLAY_WIDTH,STATUS_DISPLAY_HEIGHT);

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

    // ARGB8888 Stride is 32 bits, 32 bits per pixel, 4 uniform components of 8 bits.
    // https://en.wikipedia.org/wiki/RGBA_color_model
    //
    texture = SDL_CreateTexture(rend,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                STATUS_DISPLAY_WIDTH,
                                STATUS_DISPLAY_HEIGHT);
    if (texture == NULL) {
        fprintf(stderr, "Unable to create display texture: %s\n", SDL_GetError());
        return 1;
    }
    // colour is red, green, blue, alpha 
    // the alpha value used to draw on the rendering target; usually SDL_ALPHA_OPAQUE (255).
    //  Use SDL_SetRenderDrawBlendMode to specify how the alpha channel is used
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
    SDL_RenderClear(rend);
    SDL_RenderPresent(rend);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(rend, STATUS_DISPLAY_WIDTH, STATUS_DISPLAY_HEIGHT);

    return 0;

}

void status_display_change_size(int sizefactor){

    // since we build the display at 2x actual pixel size 
    // scaling down if actually asking for scaling of 1
    SDL_SetWindowSize(screen, STATUS_DISPLAY_WIDTH*sizefactor*.25, STATUS_DISPLAY_HEIGHT*sizefactor*.25);
    
}

void status_display_position(int x, int y){

    SDL_SetWindowPosition(screen, x, y);

}

// get the current size of the nascom window on the screen
// updates the integers pointed to by w and h
void status_GetWindowSize(int* w, int* h){

    SDL_GetWindowSize(screen,w,h);
    if (w==NULL){
        *w=STATUS_DISPLAY_WIDTH;
    }
    if (h==NULL){
        *h=STATUS_DISPLAY_HEIGHT;
    }
}




// this is called when the simz80 code calls the sim_delay function

void status_display_refresh(void)
{

    bool dirty = needsrefresh;
    needsrefresh=0;
    
    // every x cycles force a refresh the display
    static int countdown = STATUS_DISPLAYFORCEREFRESH;
    if ((countdown--)<1){
        countdown = STATUS_DISPLAYFORCEREFRESH;
        dirty=true;
    }

//    status_display_show_chars("First line",0,0);
//    status_display_show_chars("Second  line",0,1);
    // char strBuffer[100];
    // display serial input details

    displaytapestatus();

    //sprintf(strBuffer,"Serial in: %d %c input:%06ld  output:%06ld ",serial_input_available, " *"[tape_led] , tape_in_pos, tape_out_pos ); 

    //status_display_show_chars_full(strBuffer,0,STATUS_DISPLAYLINES-1,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_BLUE,STATUS_BLACK);

    //sprintf(strBuffer,"Tape pos: %d %c input:%06ld  output:%06ld ",serial_input_available, " *"[tape_led] , tape_in_pos, tape_out_pos ); 

    //status_display_show_chars_full(strBuffer,0,STATUS_DISPLAYLINES-1,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_BLUE,STATUS_BLACK);
    //status_display_show_string("Hello",10,10);
    //status_display_show_string_full("next line",10,10+STATUS_FONT_H,2,2,STATUS_RED,STATUS_GREEN);
    //ztatus_display_show_string_full("and next line",10,10+(STATUS_FONT_H*4),3,3,STATUS_BLUE,STATUS_BLACK);

    if (dirty) {
        SDL_Rect sr;
        sr.x = 0;
        sr.y = 0;
        sr.w = STATUS_DISPLAY_WIDTH;
        sr.h = STATUS_DISPLAY_HEIGHT;
        // convert pixel data into a "texture" think 4 bytes per pixel 
        SDL_UpdateTexture(texture, NULL, pixmap, STATUS_DISPLAY_WIDTH * 4 );
        // remove current picture
        SDL_RenderClear(rend);
        // create a new picture to display 
        SDL_RenderCopy(rend, texture, NULL, &sr);
        // display it - replace current frame with new data
        SDL_RenderPresent(rend);
    }
}


/*
 * display characters on the status screen
 * using character position
 */
void status_display_show_chars(char * stringdata, unsigned int xcharpos, unsigned int ycharpos)
{
    
    status_display_show_chars_full(stringdata,xcharpos,ycharpos,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,0xFFFFFFFF,0x0);

}
/*
 * display a string on the screen using character position value x and y 
 *   and allow setting of scale and colour
 * 
 */
void status_display_show_chars_full(char * stringdata, unsigned int xcharpos, unsigned int ycharpos, 
        unsigned fontxscale, unsigned fontyscale,  uint32_t charcolour, uint32_t backgroundcolour)
{
    int xpixelpos = STATUS_DISPLAY_X_OFFSET+(xcharpos * STATUS_FONT_W * fontxscale);
    int ypixelpos = (STATUS_DISPLAY_Y_OFFSET)+(ycharpos*STATUS_FONT_H*fontyscale);
    
    //printf("character at x:%d y:%d\n",xpixelpos,ypixelpos);
    status_display_show_string_full(stringdata,xpixelpos,ypixelpos,fontxscale,fontyscale,charcolour,backgroundcolour);
}
/*
 * display a string on the screen using pixel position value x and y 
 * 
 */

void status_display_show_string(char * stringdata, unsigned int xpixelpos, unsigned int ypixelpos)
{

    status_display_show_string_full(stringdata,xpixelpos,ypixelpos,1,1,0xFFFFFFFF,0x0);
}
/*
 * display a string on the screen using pixel value x and y 
 *   and allow setting of scale and colour
 * 
 */
void status_display_show_string_full(char * stringdata, unsigned int xpixelpos, unsigned int ypixelpos, 
        unsigned fontxscale, unsigned fontyscale,  uint32_t charcolour, uint32_t backgroundcolour)
{

    unsigned int xpos=xpixelpos;
    unsigned int ypos=ypixelpos;
    
    needsrefresh=1;
    if (xpos >= STATUS_DISPLAY_WIDTH){
        fprintf(stderr,"x pos %d out of bounds %d\n",xpos,STATUS_DISPLAY_WIDTH);
        return;
    }
    if (ypos >= STATUS_DISPLAY_HEIGHT) {
        fprintf(stderr,"y pos %d out of bounds %d\n",ypos,STATUS_DISPLAY_HEIGHT);
        return;
    }
    
    // Where in the pixel map to store the first character on the screen
    int characterpos = 0;
    while (stringdata[characterpos]!=0){

        // current character in screen ram
        BYTE screenByte = (stringdata[characterpos]);
        
        // get where to start in the pixmap
        // the first byte is to be diaplayed here 
        uint32_t *pixmapAddress = pixmap + (ypos * STATUS_DISPLAY_WIDTH ) + xpos;
        
        // get the address of the first line of the font 
        uint8_t *fontAddress = map80VFCcharRom1 + (STATUS_BYTESPERCHARACTER * screenByte);

        // now process the lines of the font
        for (int y = 0; y < STATUS_FONT_H; y++) {
            // doing 1 row of the characters pixels
            uint8_t fontLine = *fontAddress;

            // check if we need to do more than 1 line per font line i.e. if scaling
            for (int scaleyetc = 0 ; scaleyetc < fontyscale ; scaleyetc++ ){
                // now do one line of 8 pixels
                for (int x = STATUS_FONT_W - 1; x >= 0; x--){
                    // doing a pixel
                    // check if we need to do more than 1 pixel - i.e. if scaling
                    for (int scalexetc = 0 ; scalexetc < fontxscale ; scalexetc++ ){
                        //printf("pixel x %2.2X y %2.2X \n",x,y);
                        *pixmapAddress++ = (fontLine & (1 << x)) ? charcolour : backgroundcolour;
                    }
                }
                pixmapAddress += STATUS_DISPLAY_WIDTH - (STATUS_FONT_W*fontxscale); // minus 8 as just done 8 bits * scaling !
            }
            fontAddress++;
        }
        characterpos++;
        xpos+=STATUS_FONT_W * fontxscale;
    
    }
}




// end of file

