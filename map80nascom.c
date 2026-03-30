/*   Virtual Nascom, a Nascom II emulator.

     Copyright (C) 2000,2009,2017,2018  Tommy Thorn

     Z80 emulator portition Copyright (C) 1995,1998 Frank D. Cringle.

     NasEmu is free software; you can redistribute it and/or modify it
     under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
     02111-1307, USA.


   A Nascom consists of:

    - a Z80 CPU,
    - a UART,
    - a bitmapped keyboard,
    - memory:
        0000 - 07ff  2 KB ROM monitor,
        0800 - 0bff  1 KB screen memory,
        0c00 - 0fff  1 KB workspace
        1000 - dfff       memory
        e000 - ffff  8 KB of MS Basic

  With the Z80 emulator in place the first thing to get working is the
  screen memory.  The "correct" way to simulate screen memory is to
  trap upon writes, but that would be slow.  We do it any just to get
  started.

  DA changes
  Couple of "fixes" in the simz80.c to correct couple of bugs
        also to check the B register in some of the looping commands.
  Add NMI/Single step code to the start of the cycle in simz80.c

  added test for single step setting in the out port routine.

  changed the PutBYTE routine so it only protects the ROM monitor.
  change the PutWORD routine so it does PutBYTE twice.

  added Memory Management Unit (MMU) based om MAP80 256k Ram card.

 *  This code contains the main function plus handles all initial port inputs and outputs
 * 
*/

#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <unistd.h>
#include <stdio.h>

#include "options.h"  //defines the options to usemap80RamIntialise
#include "simz80.h"
#include "map80nascom.h"
#include "map80ram.h"
#include "display.h"
#include "map80VFCfloppy.h"
#include "nascom4SD.h"
#include "map80VFCdisplay.h"
#include "sdlevents.h"
#include "nasutils.h"   // define load program for .nas files
#include "biosmonitor.h"
#include "cpmswitch.h"
#include "statusdisplay.h"
#include "chsclockcard.h"
#include "serial.h"
#include "utilities.h"

/*
 *  global variables
 *
 */

int vfcboot=0;   // set to 1 if cpm mode set
                 // vfc boot rom in at 0 to start
                 // Nascom ROM and VWRAM not enabled
int shownascomscreen=0; // set to 1 to show nascom screen
int showVFCscreen=0; // set to 1 to display VFC Screen

bool go_fast = false;
int t_sim_delay = SLOW_DELAY;

/* NMI controls */
int singleStep;		// set to 4 to execute some instructions before triggering NMI
int NMI_flag;		// set to 1 to trigger NMI

/* Z80 registers */

WORD af[2];                     /* accumulator and flags (2 banks) */
int af_sel;                     /* bank select for af */

struct ddregs regs[2];          /* bc,de,hl */
int regs_sel;                   /* bank select for ddregs */

WORD ir;                        /* other Z80 registers */
WORD ix;
WORD iy;
WORD sp;
WORD pc;
WORD IFF;


//BYTE ram[RAMSIZE*1024];


int verbose=0;          // set to true to display messages
int traceon=0;          // set to 1 to turn trace z80 on
                        // set by f2
int tracestartaddress=0;    // trace will only show results when the PC is within this range
int traceendaddress=0xFFFF; //  end address of the trace range


int usebiosmonitor=0;   // set to 1 to make use of the bios moitor

int scaledisplays=2;    // default scale display 
/*
 *
 * end of globals
 *
 */


/*
 * define internal functions
 */

static void save_nascom(int start, int end, const char *name);
// static void reportdisplaymodes(void);
static int setdisassemblerrange(char * valuerange);

int sim_delay(void)
{
    sim_action_t localaction = CONT;

    // update the status display
    status_display_refresh();
    if (shownascomscreen!=0){
        // update the nascom display
        nascom_display_refresh();
    }
    if (showVFCscreen!=0){
        // update the vfc display
        map80vfc_display_refresh();
    }
    
    if (!go_fast){
        SDL_Delay(50);
    }
    
    //need some way to say stop from terminal ??
//    int c;
//    if ((c=getchar())=='x' ){
//        printf("x found\n");
//        action=DONE;
//    }

    // fix DA - added keyboard routine here
    // else unless port 0 keyboard refresh is issued
    // then you cannot stop the emulation
    // called to just check for control keys
    // TODO maybe need to have 2 keytables
    // 1 to set and to copy to another one on keyboard reset
    ui_serve_input();

    // action will be set by the keyboard routines to
    // DONE to close
    // RESET to restart the system
    // fix DA - when action processed by z80sim program action is never reset
    // so it keeps resetting ?
    // return current value and reset global variable
    localaction = action;
    action=CONT;
    return localaction;

}

// decode the range supplied from:to 

static int setdisassemblerrange(char * valuerange){

    // printf("l option %s\n",valuerange);
    int retval = 0;
    //unsigned int startaddress=0;
    //unsigned int endaddress=0xFFFF;

    char *currentposition = valuerange;
    
    if ( *currentposition!=':' ){
        // calculate start value 
        // need to pass address of pointer (&) as we are left at the next character
        
        tracestartaddress = hextoint(&currentposition);
    }
    if ( *currentposition==':' ){
        // we have an end value 
        currentposition++;
        if ( *currentposition!=0 ){
            // need to pass address of pointer (&) as we are left at the next character
            traceendaddress = hextoint(&currentposition);
        }
    }
    if ( *currentposition!=0 ){
        printf("unexpect values in option %s\n",currentposition);
        retval=1;
    }

    printf("Tracing only between address %4.4X and %4.4X (hex) \n",tracestartaddress,traceendaddress);

    return retval;
}



static void usage(char * progname)
{
    fprintf(stderr,
 "This is MAP80 Nascom.  Usage: %s {flags} files\n"
 "        {flags}\n"
 "           -i <file>        take serial port input from file (if tape led is on)\n"
 "           -o <file>        send serial port output to file\n"
 "           -m <file>        use <file> as monitor (default is nassys3.nal)\n"
 "           -b               boot in cpm mode\n"
 "           -f <configfile>  load floppy drive - repeat for up to 4 drives\n"
 "           -s factor        change the window sizes - default factor is 2\n"
 "           -t               output a trace of the Z80 opcodes executed ( also F2 )\n"
 "           -l  start:end    limits the trace process to with an address range\n"
 "           -v               be verbose\n"
 "           -x               use bios monitor when starting and stopped\n"
 "       files                a list of nas files to load\n"
 
            ,progname);
    exit (1);
}


int main(int argc, char **argv){

    int c;

    char *monitor;      // name of the monitor rom
    char *progname;     // name of program

    char *vfcromname;   // name of the vfc rom
    
    char *floppy[4];
    int numberofFloppies = -1;

    char *sdcard = NULL;
    int SDCardPresent = 0;

    char firstcommand[]="E0";  // used as the first command if not using biosmonitor 

// fix DA moved roms to the roms folder
    monitor = "roms/nassys3.nas";
    vfcromname = "roms/vfcrom0.nas";   // name of the vfc rom based at zero

    progname = argv[0];
    
    // will display the display modes avaiable
    //printf("display modes\n");
    //reportdisplaymodes();
    // it returns ? if invalid option having reported invalid option
    while ((c = getopt(argc, argv, "c:f:i:m:o:s:vbtxl:")) != EOF)
        switch (c) {
        case 'l':
            if (setdisassemblerrange(optarg)==1){
                // problem with the limit range values
                printf("Invalid -l options \n");
                exit (1);
            }
            break;    
        case 'i':
            setserialinputfile(optarg);
            break;
        case 'o':
            // serial output file - appends to it 
            setserialoutfilename(optarg);
            break;
        case 'b':
            vfcboot=1;
            break;
        case 't':
            traceon=1;
            break;
        case 'm':
            monitor = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'f':
            if (numberofFloppies>4){
                printf("only 4 floppies can be mounted\n");
            }
            else{
                numberofFloppies++;
                floppy[numberofFloppies]=optarg;
            }
            break;
        case 'c':
            if (SDCardPresent){
                printf("only 1 SDcard can be mounted\n");
            }
            else{
                SDCardPresent = 1;
                sdcard=optarg;
            }
            break;
        case '?':
            usage(progname);
            break;
        case 'x':
            usebiosmonitor=1;
            break;
        case 's':{
            // scale the screen by using the SDL_SetWindowSize
            int scalevalue=0;
            sscanf(optarg, "%d", &scalevalue);
            if (scalevalue >0 && scalevalue < 10 ){
                scaledisplays=scalevalue;
            }
            else{
                printf("Scale value of %d is not valid\n",scalevalue);
            }
            break;
            }
        }

    // fix DA - added F1 triggers NMI

    puts("MAP80 Nascom, a Nascom 2 emulator version " VERSION "\n"
         "with MAP80 256 ram, MAP80 VFC display and floppy card\n"
         "and CHS clock card.\n");
    if (verbose){
         puts("Copyright (c) David Allday 2021\n"
         "Uses code base from Virtual Vascom \n"
         "Copyright (C) 2000,2009,2017,2018  Tommy Thorn.\n"
         "http://github.com/tommythorn/virtual-nascom.git\n"
         "Uses software from Yet Another Z80 Emulator version "YAZEVERSION
         ", Copyright (C) 1995,1998 Frank D. Cringle.\n"
         "MAP80 Nascom comes with ABSOLUTELY NO WARRANTY;\n"
         "see the file \"COPYING\" in the distribution directory.\n"
         "\n"
         "In NASSYS mode the emulator dumps the memory state in `nasmemorydump.nas`\n"
         "upon exit so one might resume execution later on.\n"
         "\n"
         "All serial output is appended to serial output file ('-o' option)\n"
         "which may be fed back in on a subsequent launch via the '-i' option.\n"
         "\n");
    }

    if (verbose){
        char cwd[FILENAME_MAX]; //create string buffer to hold path
        getcwd( cwd, FILENAME_MAX );
        printf("Current directory is '%s'\n",cwd);
    }
    
    if (sdl_initialise()){
        // setup SDL
        fprintf(stderr,"failure to initialise SDL \n");
        exit(4);
    }   // setup SDL

    // sets the rampagetable entries to the first 64k of the ram space
    map80RamInitialise();

    // if nascom mode set the nascom2 rom and ram
    //
    setcpmswitch(vfcboot);

    //   printf("NascomMonVWram address %p \n",&NascomMonVWram);
    //   printf("NascomMonVWram screen address %p \n",&NascomMonVWram[0x800]);
    
    // set the positions of the various screens
    // 
    if (vfcboot==0){
        // use the standard Nascom display
        nascomdisplayxpos=NASCOM_DISPLAY_XPOS;
        nascomdisplayypos=NASCOM_DISPLAY_YPOS;
        statusdisplayxpos=nascomdisplayxpos+NASCOM_DISPLAY_WIDTH+10;
        statusdisplayypos=STATUS_DISPLAY_YPOS;
        shownascomscreen=1;
    }
    else{
        // use the vfc display
        map80vfcdisplayxpos=MAP80VFC_DISPLAY_XPOS;
        map80vfcdisplayypos=MAP80VFC_DISPLAY_YPOS;
        statusdisplayxpos=map80vfcdisplayxpos+MAP80VFCDISPLAY_DISPLAY_WIDTH+10;
        statusdisplayypos=STATUS_DISPLAY_YPOS;
        showVFCscreen=1;
       
    }

    // setup the status screen
    if (status_create_screen( (&statusdisplayram[0]) )){
        return 1;
    }
    // scale it to match others 
    status_display_change_size(scaledisplays);

    if (shownascomscreen!=0){
        // create nascom screen in it's own ram
        // screen is in the nascom ram space
        if (nascom_create_screen( (&NascomMonVWram[0x800]) )){
            return 1;
        }
        nascom_display_change_size(scaledisplays);
        int currentwidth=0;
        int currentheight=0;
        nascom_GetWindowSize(&currentwidth,&currentheight);
//        if (verbose){
//            printf("current Nascom screen width: %d height: %d \n",currentwidth,currentheight);
//       }
        status_display_position(nascomdisplayxpos+currentwidth+10, STATUS_DISPLAY_YPOS) ;
    }

    if (showVFCscreen!=0){
        // create vfc screen in it's own ram
        if (map80vfc_create_screen( (&vfcdisplayram[0]) )){
         return 1;
        }
        map80vfc_display_change_size(scaledisplays);
        int currentwidth=0;
        int currentheight=0;
        map80vfc_GetWindowSize(&currentwidth,&currentheight);
//        if (verbose){
//            printf("current Map80 screen width: %d height: %d \n",currentwidth,currentheight);
//        }
        status_display_position(map80vfcdisplayxpos+currentwidth+10, STATUS_DISPLAY_YPOS) ;
    }


    //load_nascom(monitor);
    // load nas monitor into the first 2k of NascomMonVWram ram and ensure it is only 2048 bytes
    if (loadNASformatspecial(monitor, &NascomMonVWram[0], 2048 ) != 0 ){
        if (vfcboot==0){ // if in cpm mode ignore error
            fprintf(stderr,"Failure in loading %s \n",monitor);
            exit (1);
        }
    }

    // load VFC Boot rom into the first 2k of vfcrom rom and ensure it is only 2048 bytes
    if (loadNASformatspecial(vfcromname, &vfcrom[0], 2048 ) != 0 ){
        if (vfcboot==1){ // if not in cpm mode ignore error
            fprintf(stderr,"Failure in loading %s \n",vfcromname);
            exit (1);
        }
    }

/*
    // fix DA moved roms to the roms folder
    // load the basic rom
    if (load_nascom("roms/basic.nal")==0){
        if (verbose){
            fprintf(stdout, "loaded basic\n");
        }
    }

    // fix DA loaded polydos into ram
    if (load_nascom("roms/PolyDos_2_Boot_ROM.nas")==0){
        if (verbose){
            fprintf(stdout, "loaded polydos2\n");
        }
    }

*/
    for (; optind < argc; optind++){
        // load it into the first 64k of the standard memory 
        // This works as the ramdefaultpagetable[0] points at the start of virtual memory
        // and virtual memory is contiguous.
        int retval=loadNASformat(argv[optind]);
        if (retval){
                printf("Problem loading %s\n",argv[optind]);
        }
    }

    // ensure all drives are reset
    resetalldrives();
    
    // now mount the drives requested
    for (int floppynumber=0;floppynumber<=numberofFloppies;floppynumber++){
        floppyMountDisk(floppynumber,floppy[floppynumber]);
    }
    
    
    displayfloppydetails();

    if (SDCardPresent) {
        SDMountDisk(sdcard);
    }

        // call to set the vfc rom\display entries
        // cpmswitchstate (set from vfcboot) acts like link4 on the hardware
        // if set :
        //      00 will enable rom and 02 disables rom
        // if not set:
        //      00 will disable rom and 02 enables rom
        // 01 enables display
        outPortVFCDisplay(0xEC,0x00);
//    }


    // fix DA - no longer needed as getword changed
    // ram[0x10000] = ram[0]; // Make GetWord[0xFFFF) work correctly

    // clear the first command if we want to use the bios monitor.
    if (usebiosmonitor!=0){
        firstcommand[0]=0;
    }

    MAP80nascomMonitor(firstcommand);

    if (cpmswitchstate==0){
        // save the nascom space to file
        save_nascom(0x800, 0x10000, "nasmemorydump.nas");
    }
    exit(0);
}


/* see .h file for details of the port usage
 *  Port 00 - keyboard, tape and single step
 *  Port 01 - 02 - Uart processing
 *  Port 04 - PIO port A data input and output
 *  Port 05  PIO port B data input and output
 *  Port 06  PIO port A control
 *  Port 07  PIO port B control
 *  Port E0-EF Map80 VFC card
 *  Port 0xFE Map 80 256k card controls memory mapping
 *
 * TODO - the real Nascom repeats the ports 00 to 0F at 10 to 1F etc., till 70 to 7F
 * 
*/


void out(unsigned int port, unsigned char value)
{
    static int SingleStepState;  // set to 1 after single step activated so we dont do it again.
    static int sscount;  // used to display ss details so know new message output


    // change to (1) to display message
    if (0) fprintf(stdout, "Out to port %02x value %02x\n", port, value);

    if ( (port & 0xF0) == 0 ) {
        switch (port & 0x0F) {
        case 0:

            // keyboard out routine
            outPort0Keyboard(value);

            if (verbose){
                if (tape_led != !!(value & P0_OUT_TAPE_DRIVE_LED))
                    fprintf(stderr, "Tape LED = %d\n", !!(value & P0_OUT_TAPE_DRIVE_LED));
            }

            tape_led = (!!(value & P0_OUT_TAPE_DRIVE_LED)) | tape_led_force;

        // check if P0_OUT_SINGLE_STEP bit value has been set ( bit 3 value 8 )
            if ( (value & P0_OUT_SINGLE_STEP) == P0_OUT_SINGLE_STEP ) { // single step bit is set ( 8 )
                    // only process if just gone from zero to 1
                if ( SingleStepState == 0 ) {
                        sscount +=1;
                    // fprintf(stdout,"Single Step triggered %d \n",sscount);
                        // set single step
                        SingleStepState = 1;
                        singleStep = 4; // trigger single step after ? instructions
                    }
            } else {   // single step switch reset
                if ( SingleStepState == 1 ) {
                    sscount +=1;
                    // fprintf(stdout,"Single Step reset %d \n",sscount);
                    SingleStepState = 0;
                   }
            }

            break;

        case 1:
            writeserialout(value);
            break;
        default:
            if (verbose){
                fprintf(stdout, "Unknown output to port %02x value %02x\n", port, value);
            }
        }
    }
    else {
        switch (port & 0xFF) {
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
            outPortSD( port,value);
            break;

        // clock card PIO ports - for Chris's version
        case 0x88:
            crtc_PIOportadata_write( port,value);
            break;
        case 0x8B:
            crtc_PIOportbdata_write( port,value);
            break;
        case 0x8C:
            crtc_PIOportacontrol_write( port,value);
            break;
        case 0x8D:
            crtc_PIOportbcontrol_write( port,value);
            break;


        // clock card PIO ports 
        case 0xD0:
            crtc_PIOportadata_write( port,value);
            break;
        case 0xD1:
            crtc_PIOportbdata_write( port,value);
            break;
        case 0xD2:
            crtc_PIOportacontrol_write( port,value);
            break;
        case 0xD3:
            crtc_PIOportbcontrol_write( port,value);
            break;

            // floppy handling
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
            outPortFloppy(port,value);
            break;
            // VFC screen handling
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
            outPortVFCDisplay(port,value);
            break;

            // MAP80 256k Card
        case 0xFE:
            // handle page mapping
           map80Ram(value); 
           //printf("port out 0xFE disabled - value %2.2X\n",value);
            break;

        default:
            if (verbose){
                fprintf(stdout, "Unknown output to port %02x value %02x\n", port, value);
            }
        }
    }
}



int in(unsigned int port)
{

    int retval=0xFF;
    
    if ( (port & 0xF0) == 0 ) {
        switch (port & 0x0F) {
        case 0:
            retval=inPort0Keyboard();
            break;
            
        case 1:
            retval=readserialin();
            break;

        case 2:{
            // check if any data to get from serial file 
            /* Status port on the UART */
            // 
            retval=getuartstatus();
                //int uart_status= UART_TBR_EMPTY | (checkifanyserialinputdata() & tape_led ? UART_DATA_READY : 0);
                // printf("Uart status check %2.2X, serial ready %2.2X, tape led %2.2X, \n",uart_status,serial_input_available,tape_led);
                //return uart_status;
            }
            break;
        default:
            retval= 0xFF;
            if (verbose){
                fprintf(stdout, "unknown input request from port %2.2X returning %2.2X\n", port,retval);
            }
            break;
        }
    }
    else {
        switch (port & 0xFF) {

        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
            retval=inPortSD(port);
            break;

            // clock card PIO ports - for Chris's version
        case 0x88:
            retval=crtc_PIOportadata_read(port);
            break;
        case 0x89:
            retval=crtc_PIOportbdata_read(port);
            break;
        case 0x8A:
            retval=crtc_PIOportacontrol_read(port );
            break;
        case 0x8B:
            retval=crtc_PIOportbcontrol_read( port );
            break;


        // clock card PIO ports 
        case 0xD0:
            retval=crtc_PIOportadata_read(port);
            break;
        case 0xD1:
            retval=crtc_PIOportbdata_read(port);
            break;
        case 0xD2:
            retval=crtc_PIOportacontrol_read(port );
            break;
        case 0xD3:
            retval=crtc_PIOportbcontrol_read( port );
            break;


            // handle the floppy input ports
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
            retval=inPortFloppy(port);
            break;
            // VFC screen handling
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
            retval=inPortVFCDisplay(port);
            break;

        default:
            retval= 0xFF;
            if (verbose){
                fprintf(stdout, "unknown input request from port %2.2X returning %2.2X\n", port,retval);
            }
            break;
        }
    }

    if (0) fprintf(stdout, "In from Port %2.2X value %2.2X\n", port,retval);

    //if ( (port & 0xf0) == 0xe0) fprintf(stdout, "in [%02x] \n", port);
    return retval;

}




static void save_nascom(int start, int end, const char *name)
{
    printf("Dumping memory from %4.4X to %4.4X to file %s\n",start,end,name);
    FILE *f = fopen(name, "w+");

    if (!f) {
        perror(name);
        return;
    }
    // save as a nascom style file with csum
    for (unsigned int  address = start;address<end;address+=8){
        unsigned int checksum=0;
        fprintf(f, "%4.4X ",address);
        for (unsigned int byteno=0;byteno<8;byteno++){
            checksum=(checksum+RAM(address+byteno))& 0xFF;
            fprintf(f,"%2.2X ",RAM(address+byteno));
        }
        fprintf(f, "%2.2X  %c%c\r\n",checksum,8,8);
    }
    
//    for (uint8_t *p = &RAM(0) + start; start < end; p += 8, start += 8)
//        fprintf(f, "%04X %02X %02X %02X %02X %02X %02X %02X %02X %02X%c%c\r\n",
//                start, *p, p[1], p[2], p[3], p[4], p[5], p[6], p[7], 0, 8, 8);

    fclose(f);
}



/*
static void reportdisplaymodes(void){

  int i;

  // Declare display mode structure to be filled in.
  SDL_DisplayMode current;

  SDL_Init(SDL_INIT_VIDEO);

  // Get current display mode of all displays.
  for(i = 0; i < SDL_GetNumVideoDisplays(); ++i){

    int should_be_zero = SDL_GetCurrentDisplayMode(i, &current);

    if(should_be_zero != 0)
      // In case of error...
      printf("Could not get display mode for video display #%d: %s\n", i, SDL_GetError());

    else
      // On success, print the current display mode.
      printf("Display #%d: current display mode is %dx%dpx @ %dhz.\n", i, current.w, current.h, current.refresh_rate);

  }


}
*/

