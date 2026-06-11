/*
 *   module to define SDL events stuff
 *
 *   and also handle the keyboard events
 *     and Z80 port 0 keyboard stuff
 *
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
#include "sdlevents.h"
#include "serial.h"

// set values done in map80nascom.c
//int showkeymatrix=SHOWKEYMATRIX;
//int displaykeyvalues=DISPLAYKEYVALUES;

// define which key processing routine is used.
// DA N4 added shiftdowmn so we can use more function keys
static void handle_key_event_dwim(SDL_Keysym keysym, bool keydown,bool shiftdown);
static void handle_key_event_raw(SDL_Keysym keysym, bool keydown,bool shiftdown);

// fix DA changed start keyboard type
static void (*handle_key_event)(SDL_Keysym, bool, bool ) = handle_key_event_dwim;
static int rawkeyboard=0; // set to 1 if using raw keyboard

// SDL initialize everything

int sdl_initialise(void){

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        return 1;
    }

    // on exit
    atexit(SDL_Quit);

    return 0;
}


// service ui input processing
// service the SDL event processing
// all screens generate events in the SDL_Event queue
//
void ui_serve_input(void)
{
    SDL_Event event;

    // fix DA change to a while to get all events ( using while )
    // multiple key strokes will generate multiple events
    // e.g. shift and E

    while (SDL_PollEvent(&event)) {
//        printf("SDL event %d\n", event.type);

        switch (event.type) {
        case SDL_MOUSEMOTION:
            /*printf("Mouse moved by %d,%d to (%d,%d)\n",
              event.motion.xrel, event.motion.yrel,
              event.motion.x, event.motion.y);*/
            break;
        case SDL_MOUSEBUTTONDOWN:
            /*printf("Mouse button %d pressed at (%d,%d)\n",
              event.button.button, event.button.x, event.button.y);*/
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            // key up or key down event
            // send the keycode, true if keydown event else false
            // DA how to identify if shift down ????
            // TODO 
            handle_key_event(event.key.keysym, event.type == SDL_KEYDOWN,false);
            break;
        case SDL_QUIT:
            printf("Quit\n");
            action = DONE;
            return;
        case SDL_WINDOWEVENT:
            // handle a window close on a multiple window setup
            if (event.window.event == SDL_WINDOWEVENT_CLOSE){
                action=DONE;
                return;
            }
            break;
        default:
//            printf("Unknown event: %d\n", event.type);
            break;
        }
    }
}

/* The keyboard holds the state state of every depressed key and a
   current scanning pointer.

   fix DA it should only be 8 rows  not 9
   NASSYS3 scans 9 rows but it is really 0 to 7 then back to 0
   so what is mentioned on row 8 should be in row 0

 * created 2 matrix as needed to scan SDL events more times
 * and it sometimes changed the matrix as it was being processed ny NASSYS
 * 
   */

static struct keymap {
    unsigned char mask[8];
    unsigned char index;
} keyboard  = {
    {   0,   /* Ch @  Sh Ct -  Nl Bs */
        0,  /* Up T  X  F  5  B  H  */
        0,  /* Lt Y  Z  D  6  N  J  */
        0,  /* Dn U  S  E  7  M  K  */
        0,  /* Rt I  A  W  8  ,  L  */
        0,  /* Gr O  Q  3  9  .  ;  */
        0,  /* [  P  1  2  0  /  :  */
        0  /* ]  R  Sp C  4  V  G  */
    },
    0};

struct keymap keyboardcopy =  {
    {
        0,   /* Ch @  Sh Ct -  Nl Bs */
        0,  /* Up T  X  F  5  B  H  */
        0,  /* Lt Y  Z  D  6  N  J  */
        0,  /* Dn U  S  E  7  M  K  */
        0,  /* Rt I  A  W  8  ,  L  */
        0,  /* Gr O  Q  3  9  .  ;  */
        0,  /* [  P  1  2  0  /  :  */
        0  /* ]  R  Sp C  4  V  G  */
    },
    0};

/*
static char * kbd_translation[] = {
 0   "________",
 1   "__TXF5BH",
 2   "__YZD6NJ",
 3   "__USE7MK",
 4   "__IAW8,L",
 5   "__OQ39.;",
 6   "_[P120/:",
 7   "_]R C4VG",
 8   "_\t@__-\r\010"
};
*/

// fix DA change translation table
// this gives the row and column(bit) for each character - uppercase only
// column 7 (i.e. bit 7 ) is not used by the N2 keyboard - pity really but . . . .
// column 6 is mainly cursor control characters
// row 0 is the tricky one to see \t = tab = 0x08  \r = CR = 0x0D == enter and  \010 == backspace
// \t is tab 0x09
// \r is carrage return 0xD
// \010 is 8 backspace

static char * kbd_translation[] = {
/* 0 */  "_\t@__-\r\010",
/* 1 */  "__TXF5BH",
/* 2 */  "__YZD6NJ",
/* 3 */  "__USE7MK",
/* 4 */  "__IAW8,L",
/* 5 */  "__OQ39.;",
/* 6 */  "_[P120/:",
/* 7 */  "_]R C4VG"
};

    // if shift pressed then translate the normal key character supplied to the shifted key character
    // the char array has normal character followed by the required shifted character
    // this is for a us keyboard !!
//static const char kbd_us_shift[] = ";:'\"[{]}\\|-_=+`~1!2@3#4$5%6^7&8*9(0),<.>/?";

// this is for the us keyboard
static char kbd_us_shift[] = ";:"
                                   "'\""
                                   "[{"
                                   "]}" 
                                   "\\|" 
                                   "-_" 
                                   "=+" 
                                   "`~" 
                                   "1!" "2@" "3#" "4$" "5%" "6^" "7&" "8*" "9(" "0)" 
                                   ",<" 
                                   ".>" 
                                   "/?";

// this is for the uk keyboard
static char kbd_uk_shift[] = ";:"
                                   "'@"
                                   "[{"
                                   "]}" 
                                   "\\|" 
                                   "-_" 
                                   "=+" 
                                   "`~" 
                                   "1!" "2\"" "3#" "4$" "5%" "6^" "7&" "8*" "9(" "0)" 
                                   ",<" 
                                   ".>" 
                                   "/?";

static char * kbd_xx_shift=NULL;    // will be pointed at either uk or us shift pattern.

#define ___ " " // Dummy entry in the mapping strings

//
/*
* keyboard mapping characters
*/
static const char
kbd_spec          [] = ";" ":" "["    "]" "-" "," "." "/"    "0" "1" "2"  "3" "4" "5" "6" "7" "8" "9" " " "@";
// this maps the characters that should have shift pressed to get 
// and maps it to the key above so the correct bits are set.
static const char
kbd_spec_w_shift  [] = "+" "*" "\\"   "_" "=" "<" ">" "?"    "^" "!" "\"" "#" "$" "%" "&" "'" "(" ")" ___ "@";
// this maps the characters that  need control pressed
static const char
kbd_spec_w_ctrl   [] = "{" ___ ___    ___ ___ ___ ___ ___    ___ ___ ___  ___ ___ ___ ___ ___ ___ ___ "`";
// this maps the characters that need shift and control
static const char
kbd_spec_w_shctrl [] = ___ ___ ___    ___ "}" "|" "~" "\177" ___ ___ ___  ___ ___ ___ ___ ___ ___ ___ ___;

// typedef enum { CONT = 0, RESET = 1, WARMRESET=2, DONE = -1 } sim_action_t;
sim_action_t action = CONT;

// Ctr-Shift-Meta 0 -> the REAL # (instead of the pound symbol)
// Ctrl-Space -> `

/*
   handle the Function keys
*/
static void handle_app_control(SDL_Keysym keysym, bool keydown, bool shiftdown)
{

    //fprintf(stdout,"Shift key is [%02x] \n",shiftdown);
    if (keydown)
        switch (keysym.sym) {
        case SDLK_END: {
            FILE *f;
            // TODO sort out screen dump
            f = fopen("screendump", "a+");
            printf("byte at 0x800 %2.2X \n",RAM(0x800));
            printf("byte at 0x900 %2.2X \n",RAM(0x900));
            printf("pointer %p \n",&RAM(0x800));
            //fwrite((const void *) RAM(0x800), 1, 1024, f);
            // need the address of the RAM
            fwrite((const BYTE *) &RAM(0x800), 1, 1024, f);
            fclose(f);
            if (verbose) printf("Screen dumped\n");
            break;
        }
        // DA Fix added F1 as NMI switch
        case SDLK_F1:
            NMI_flag=1;
            break;

        case SDLK_F2:
            action = WARMRESET;
            // this will do a "warm" reset for the N4 style code
            // see code simz80 as needed to set other stuff for N4 etc.,
            printf("Warm reset requested\n");
            break;

        case SDLK_F3:
            action = RESET;
            // this will reset PC to 0 need to reset hardware
            //out(0xFE,0); // reset paging on MAP80RAM card
            //out(0xEC,0); // reset paging on MAP80VFC
            // see code simz80 as needed to set other stuff for N4 etc.,
            printf("cold reset requested\n");
            break;

        case SDLK_F4:
            // close emulator
            action = DONE;
            if (shiftdown){
                usebiosmonitor=1;
            }
            break;

        case SDLK_F5:
            // reset serial in
            resetserialinput();
            break;



        case SDLK_F6:
            tape_led = tape_led_force ^= 1;
            printf("Tape led forced on\n");
            break;

        case SDLK_F7:
            if (traceon==1){
                traceon=0;
                printf("Trace off\n");
            }
            else {
                traceon=1;
                printf("Trace on\n");
            }
            break;

        case SDLK_F8:
            go_fast = !go_fast;
            printf("Switch to %s\n", go_fast ? "fast" : "slow");
            // has no impact as t_sim_delay only used on call to simz80 ????
            t_sim_delay = go_fast ? FAST_DELAY : SLOW_DELAY;
            break;

        case SDLK_F9:
                if (handle_key_event == handle_key_event_raw){
                    handle_key_event = handle_key_event_dwim;
                    rawkeyboard=0;
                }
                else{
                    handle_key_event = handle_key_event_raw;
                    rawkeyboard=1;
                }
                printf("Switch to %s keyboard\n",
                       handle_key_event == handle_key_event_raw ? "raw" : "dwim");
                break;

        default:
            ;
        }
}


/*
*  This handles the keyboard and set values as shown on a standard keyboard
*  rather than those on a nascom keyboard.
*   So the shifted version of keys
*    2 to 0 are as per standard keyboard
*       which is 1 2 3 4 5 6 7 8 9 0
*  us            ! @ # $ % ^ & * ( )
*  uk            ! " £ $ % ^ & * ( )
*
*  It then translate these values into the nascom keytable
*  fix DA - did some changes to the way this works
*   may not be better but was having issues with characters following shifted characters
*   especially BS was being treated as CS
*  Set the graph and control key bits in the key matrix when they are pressed
*  rather than waiting for another key to be pressed.
*  This routine uses it's own keyboard matrix which is then 
*    copied to the one Nascom is using when NASSYS does a keyindex reset.
*  It usd to call the keyboard routines only when it did a keyindex reset
*  but this meant if it went into a loop you could not stop it. 
*  keyboard i/o (actually SLD_events ) is called during the screen refresh 
*  but that had some interesting effects on the key input so used 2 keyboard matrixes 
* 
*/

static void handle_key_event_dwim(SDL_Keysym keysym, bool keydown, bool shiftdown)
{
    int i = -1, bit = 0;
    // these are the states from the "real" keyboard
    static bool ui_shift = false;
    static bool ui_ctrl  = false;
    static bool ui_graph = false;
    // not sure DA - think the emu are for emulation of the nascom keyboard
    bool emu_shift = false;
    bool emu_ctrl  = false;
    bool emu_graph = false;
    int ch = toupper((uint8_t)keysym.sym);

    if (displaykeyvalues!=0){
    //printf("key event\n");
        if ( (keysym.sym  > 31) && (keysym.sym  < 128 )){
            fprintf(stdout,"dwim keyvalue [%02X] as char [%c] name [%s]  keystate [%s] - uppercase [%02X] \n",
                     keysym.sym,
                     (uint8_t)keysym.sym,
                     SDL_GetKeyName(keysym.sym),
                     keydown ? "down" : "up",
                     ch);
        }
        else {
            fprintf(stdout,"dwim keyvalue [%02X] name [%s] keystate [%s] - uppercase [%02X] \n",
                    keysym.sym,
                    SDL_GetKeyName(keysym.sym),
                    keydown ? "down" : "up",
                    ch);
        }
    }


    /* We are getting raw key code events, so first we need to handle
     * the UI a bit */

    // fix DA allowed return for shift control and graph keys
    switch (keysym.sym) {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            // printf("Shift key\n");
            ui_shift = keydown;
            return;

        case SDLK_LCTRL:
        case SDLK_RCTRL:
                    //printf("control key %d\n",keydown);
                    // set right away in matrix 
                    i=0, bit = 3;
                    ui_ctrl = keydown;
                    break;

        case SDLK_RGUI:
        case SDLK_LGUI:
        case SDLK_RALT:
        case SDLK_LALT:
                    //printf("Graph key %d\n",keydown);
                    // set in matrix
                    i = 5, bit = 6;
                    ui_graph = keydown;
                    break;
        // cursor keys
        case SDLK_UP:      i = 1, bit = 6; break;
        case SDLK_LEFT:    i = 2, bit = 6; break;
        case SDLK_DOWN:    i = 3, bit = 6; break;
        case SDLK_RIGHT:   i = 4, bit = 6; break;
        // DA fix add other enter keys
        case SDLK_KP_ENTER:                        // enter key on keypad
        case SDLK_RETURN:                          // return/enter key  main keyboard
        case SDLK_RETURN2:                         // return/enter key  main keyboard
        //            printf("returnkey found \n");
            i = 0 , bit = 1;
            break;
        case SDLK_ESCAPE:
            i = 0 , bit = 1;
            emu_shift=1;
            break;
        case SDLK_SPACE:   // space character 
            i = 7 , bit = 4;
            break;
            
        default:{

            // taken care of the basic keys 
            
            // printf("ch starts [%02X]\n",ch);

            //emu_shift = !ui_shift && isalpha((uint8_t)keysym.sym);
            // not sure why this is done - - - except
            // it gave lower case in nascom when not shifted
            // because nascom not shifted alpha is upper case
            // fix DA removed as want upper case by default
            //
            if (isalpha((uint8_t)keysym.sym)) {
                emu_shift = ui_shift;
            }
            // is this correct ?
            //
            emu_ctrl  = ui_ctrl;
            emu_graph = ui_graph;

            // if shifted then convert key value to the shifted character value on the keyboard
            // 
            if (KEYBOARDTYPE==0){
                kbd_xx_shift=kbd_us_shift;
            }
            else{
                kbd_xx_shift=kbd_uk_shift;
            }
            if (ui_shift) {
                for (int i = 0; kbd_xx_shift[i]; i += 2) {
                    if (kbd_uk_shift[i] == ch) {
                        //printf("ch shift changed %2.2X to %2.2X\n",ch,kbd_us_shift[i+1]);
                        ch = kbd_uk_shift[i+1];
                        break;
                    }
                }
            }
            
            /* Now translate the ASCII to Nascom keyboard events */


            // looking for keys that need shift set
            // looks until 0 found in the string
            for (int i = 0; kbd_spec_w_shift[i]; ++i)
                if (kbd_spec_w_shift[i] == ch) {
                    //printf("ch w_shift %d changed %2.2X to %2.2X\n",i,ch,kbd_spec[i]);
                    emu_shift = true;
                    ch = kbd_spec[i];
                    goto search;
                }

            for (int i = 0; kbd_spec_w_ctrl[i]; ++i)
                if (kbd_spec_w_ctrl[i] == ch) {
                    //printf("ch w_cntrl %d changed %2.2X to %2.2X\n",i,ch,kbd_spec[i]);
                    emu_shift = false;
                    emu_ctrl = true;
                    ch = kbd_spec[i];
                    goto search;
                }

            for (int i = 0; kbd_spec_w_shctrl[i]; ++i)
                if (kbd_spec_w_shctrl[i] == ch) {
                    //printf("ch w_cshntrl %d changed %2.2X to %2.2X\n",i,ch,kbd_spec[i]);
                    emu_shift = true;
                    emu_ctrl = true;
                    ch = kbd_spec[i];
                    goto search;
                }


            search:
            //if (keysym.sym < 128) {
            if (keysym.sym < 128) {
                // look for the key value in the translation table
                // entries 0 to 7
                for (i = 0; i < 8; ++i){
                    for (bit = 0; bit < 7; ++bit){
                        // printf ("char [%02X] == [%02X] \n",kbd_translation[i][7-bit],ch);
                        if (kbd_translation[i][7-bit] == ch) {
                            //printf ("char %2.2X found i %2.2X bit %2.2X 7-%2.2X \n",ch,i,bit,7-bit);
                            goto translate;
                        }
                    }
                }

                i = -1;
            } else {

                //emu_shift = ui_shift;

                switch (keysym.sym) {
                    default:
                        handle_app_control(keysym, keydown, ui_shift);
                        break;
                }
            }
            break;
        }
    }

translate:

//    printf("ch translated [%02X] mask [%d] bit [%d] \n",ch, i, bit);
//    see the translate table needing shift
//    if (ch== '@'){
//        emu_shift = true;
//        //i = 0 , bit = 5;
//    }

    if (emu_shift){
        keyboardcopy.mask[0] |= 1 << 4;
//        printf("shift key down\n");
    }
    else{
        keyboardcopy.mask[0] &= ~(1 << 4);
//        printf("shift key up\n");
    }
    if (emu_ctrl)
        keyboardcopy.mask[0] |= 1 << 3;
    else
        keyboardcopy.mask[0] &= ~(1 << 3);

    if (emu_graph){
        keyboardcopy.mask[5] |= 1 << 6;
    }
    else
        keyboardcopy.mask[5] &= ~(1 << 6);

    if (i != -1) {
        if (keydown){
            keyboardcopy.mask[i] |= 1 << bit;
//            printf("mark %d bit %d set\n",i,bit);
        }
        else{
            keyboardcopy.mask[i] &= ~(1 << bit);
//            printf("mark %d bit %d cleared\n",i,bit);
        }
    }
    else {
//        printf("no change\n");
    }
//    printf("\n");
}





/* The raw keyboard returns values as per the NASCOM2 keyboard.
   So the shifted version of keys 1, 2 to 0 are as per the N2 keyboard
       which is 1 2 3 4 5 6 7 8 9 0
                ! " £ $ % & ' ( ) ^
   This is because the keysym.sym value tells you the key pressed
   and not the value if it is shifted etc.,

Problem:-
   Some keys do not translate to their values as the shifted version of the key
   is not the nascom base one

   e.g.
on UK keyboard:-
    will never get the @ value as that is shifted and normal key value is '

on US keyboards
    will never get the @ value as that is shifted and normal key value is 2


*/

static void handle_key_event_raw(SDL_Keysym keysym, bool keydown, bool shiftdown)
{
    int i = -1, bit = 0;
    
    // debug don't print control code
    if (displaykeyvalues!=0){
    //printf("key event\n");
        if ( (keysym.sym  > 31) && (keysym.sym  < 128 )){
            fprintf(stdout,"dwim keyvalue [%02X] as char [%c] name [%s]  keystate [%s] \n",
                     keysym.sym,
                     (uint8_t)keysym.sym,
                     SDL_GetKeyName(keysym.sym),
                     keydown ? "down" : "up");
        }
        else {
            fprintf(stdout,"dwim keyvalue [%02X] name [%s] keystate [%s] \n",
                    keysym.sym,
                    SDL_GetKeyName(keysym.sym),
                    keydown ? "down" : "up");
        }
    }


    // We need this to be able to share kbd_translation between the
    // RAW and the DWIM variant

    // this makes the ' key the : and * nascom key
    if (keysym.sym == '\'')
        keysym.sym = ':';

    // fprintf(stdout,"keyvalue [%02X]\n",keysym.sym);

    // if key < 128 then basic key code
    // search the table for an upper case character
    // if found it tells us which bit to set.

    if (keysym.sym < 128) {
        int ch = toupper(keysym.sym);
        // fprintf(stdout,"ch value [%02X]\n",ch);
        // fix DA start search at 0 and ends at 7
        for (i = 0; i < 8; ++i)
            for (bit = 0; bit < 7; ++bit)
                if (kbd_translation[i][7-bit] == ch) {
                    //fprintf(stdout,"found key at bit [%d] for index [%d]\n",bit,i);
                    goto translate;
                }
        i = -1;
        translate:;
    } else {
        // these are the special keys like return and shift

        //fprintf(stdout,"SDLK_RETURN [%02X]\n",SDLK_RETURN);
        //fprintf(stdout,"SDLK_RETURN2 [%02X]\n",SDLK_RETURN2);
        //fprintf(stdout,"SDLK_KP_ENTER [%02X]\n",SDLK_KP_ENTER);

        // control keys
        switch (keysym.sym) {
        // case Newline  i = 0, bit = 5; break;
        // case '@':     i = 8, bit = 5; break;
        case SDLK_LCTRL:
        case SDLK_RCTRL:   i = 0, bit = 3; break;

        case SDLK_LSHIFT:
        case SDLK_RSHIFT:  i = 0, bit = 4; break;

        case SDLK_UP:      i = 1, bit = 6; break;
        case SDLK_LEFT:    i = 2, bit = 6; break;
        case SDLK_DOWN:    i = 3, bit = 6; break;
        case SDLK_RIGHT:   i = 4, bit = 6; break;

        case SDLK_LGUI:
        case SDLK_RGUI:
        case SDLK_LALT:
        case SDLK_RALT:     i = 5, bit = 6; break;
        // fix DA changed i for enter and added return key
        // actually SDLK_RETURN (ox0D) will be found in the table search above
        case SDLK_KP_ENTER:                        // enter key on keypad
        case SDLK_RETURN:                          // return/enter key  main keyboard
        case SDLK_RETURN2:
                        i = 0, bit = 1; break; // return/enter key  main keyboard
        default:
            // not one for Nascom so check if emulator control key
            handle_app_control(keysym, keydown,shiftdown);
        }
    }
    // having identified the bit required
    // either set or unset it
    //fprintf(stdout,"setting bit [%d] for index [%d]\n",bit,i);

    if (i != -1) {
        if (keydown)
            keyboardcopy.mask[i] |= 1 << bit;
        else
            keyboardcopy.mask[i] &= ~(1 << bit);
    }
}

void outPort0Keyboard(int value){

    static unsigned char port0; // value on previous call
    unsigned int down_trans;

    /* KBD */
    // DA fix also contains the single step switch on bit 3

    // ~ Binary One's Complement Operator is unary and has the effect of 'flipping' bits.
    // sets any bit where is was 1 last time but is now 0
    // so down_trans has bit set to 1 if that bit has moved from 1 to 0
    down_trans = port0 & ~value;
    // save current value for next time.
    port0 = value;

    // debug display message
    // fprintf(stdout, "Port 0: Value [%02x] down_trans %02x\n", port0, down_trans);


    // if keyboard clock bit is set add 1 to index
    // fix DA removed test for counter value
    //  and ensured counter recyles from 7 to 0
    if (down_trans & P0_OUT_KEYBOARD_CLOCK) {
        keyboard.index++;
        // fix DA there should only be 8 entries in the table 0 to 7
        // Nassys 3 does 9 scans and Polydos 10 scans ?
        // ensure it cycles round
        keyboard.index &= 7;
        // fprintf(stdout, "keyboard.index stepped to  [%02x] \n" , keyboard.index);
    }
    // reset keyboard row counter - check for key inputs
    if (down_trans & P0_OUT_KEYBOARD_RESET) {
        // does keyboard check here so all the bits are set
        // and stay until end of scan
        // problem - does not allow for control if emulator not asking for input
        // solved by just checking control keys during z80sim call to sim_delay
        // TODO but may cause issues if the keystable is updated during processing?
        ui_serve_input();
        // copy the copy keymap to this one 
        int notzero=0;
        for (int entry=0;entry<8;entry++){
            // test for display if changed 
            if (keyboard.mask[entry]!=keyboardcopy.mask[entry]){
                notzero=1;
            }
            keyboard.mask[entry]=keyboardcopy.mask[entry];
            // bug fix ---- there was  a problem with : and ; keys repeating 
            // the release of the key was not being recognised during the keyboard processing
            // so cleared keyboard matrix each time allows it to work
            // gives a problem when raw keyboard mode selected 
            if (! rawkeyboard ){
                keyboardcopy.mask[entry]=0; // clear keyboard matrix after each key search
            }
        }
        
        // debug code to show keyboard matrix
        if (showkeymatrix!=0){
            // if matrix changed then print it 
            if (notzero!=0){
                for (int entry=0;entry<8;entry++){
                    printf("%d ",entry);
                    for (int bit=7;bit>=0;bit--){
                        if (((keyboard.mask[entry]>>bit)&01)==0){
                            printf("0");
                        }
                        else{
                            printf("1");
                        }
                    }
                    printf("\n");
                }
                printf("\n");
            }
        }
        // reset the index counter for the keyboard process
        keyboard.index = 0;
    }
}

int inPort0Keyboard(){

    /* KBD */
    // debug

    if (0) { // (keyboard.mask[keyboard.index] != 0 ){
        printf("Port 0: returning [%02X] from [%d] \n",(~keyboard.mask[keyboard.index])&0xFF,keyboard.index);
    }
    return (~keyboard.mask[keyboard.index])&0xFF;

}

