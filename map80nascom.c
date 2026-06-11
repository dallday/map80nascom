/*
 * 
 *  map80nascom version 9
 * 
         Copyright (c) David Allday 2021-2026
         Uses code base from Virtual Vascom 
         Copyright (C) 2000,2009,2017,2018  Tommy Thorn.
         http://github.com/tommythorn/virtual-nascom.git
         Uses software from Yet Another Z80 Emulator version "YAZEVERSION"
         , Copyright (C) 1995,1998 Frank D. Cringle.
         MAP80 Nascom comes with ABSOLUTELY NO WARRANTY;
         see the file \"COPYING\" in the distribution directory.
         
         In NASSYS mode the emulator dumps the memory state in `nasmemorydump.nas`
         upon exit so one might resume execution later on.
         
         All serial output is appended to serial output file ('-o' option)
         which may be fed back in on a subsequent launch via the '-i' option.


     This is from http://github.com/tommythorn/virtual-nascom.git

     Virtual Nascom, a Nascom II emulator.

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

 
  changes made for map80nascom which adds mapped memory, floppy drives and vfc screens
   added the debugger routines plus the bios monitor.

  DA changes
  Couple of "fixes" in the simz80.c to correct couple of bugs
        also to check the B register in some of the looping commands.
  Add NMI/Single step code to the start of the cycle in simz80.c

  added test for single step setting in the out port routine.

  changed the PutBYTE routine so it only protects the ROM monitor.
  change the PutWORD routine so it does PutBYTE twice.

  added Memory Management Unit (MMU) based om MAP80 256k Ram card.

  version 8.1 - updated by Neal Crook for N4 (nascom4SD.c nascom4SD.h)
        added extra SCAL options to the dissassembler process
        tweeks to the map80VFCfloppy.c routines
        increased the virtual ram to 1024 k (options.h)
        Fixed the loading of .nas files
            Old code required 8 data + checksum per line. If the checksum was
            missing it would grab a value from the next line and then gobble/discard
            that line. New code accepts 1-9 bytes of data and only checksums if
            all 9 are present. Refactored to share code between both load routines.     

 * 
 * Note I used codelite to build this 
 * you need to chage the working directory from $(IntermediateDirectory) 
 * to the diredtory that contains the rom etc diretories
 * try $(ProjectPath) nope ?
 * 
 * 
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

// set to 1 to see the VFC display / rom debug info
int vfcdisplaydebug=0; 
// set to 1 to display floppy debug details
int vfcfloppydebug=0;
// set to 1 to display floppy sectors read and written
int  vfcfloppydisplaysectors=0;
// keyboard and SDL processing 
// set to 1 to show the nascom keyboard matrix each time nassys does a keyboard scan.
// displayed when it does an index reset.
int  showkeymatrix=0;
// shows the sdl key values during processing
int  displaykeyvalues=0;

// display clock card processing
int chsclockcarddebug=0;

// display nascom areas page in and out
int nascompagedebug=0;
// set to 1 to show the N4 Port changes
int N4portdebug=0;


// DA N4 no longer used now handled by the REMAP port 
// int vfcboot=0;   // set to 1 if cpm mode set
                 // vfc boot rom in at 0 to start
                 // Nascom ROM and VWRAM not enabled
// both screen actived but 1 is hidden see OEF or OEE 
//int shownascomscreen=0; // set to 1 to show nascom screen
//int showVFCscreen=0; // set to 1 to display VFC Screen
char emulator_mode = 'n';

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


//BYTE ram[RAMSIZE*1024]; see the memory file

// these are to hold the values written to for N4 ports
unsigned int Port_REMAP_value = 0;
unsigned int Port_PROTECT_value = 0;
unsigned int Port_MWAITS_value = 0;
unsigned int Port_PORPAGE_value = 0;
unsigned int Port_REASON_value = 0xC0;
unsigned int Port_SERCON_value = 0;



int verbose=0;          // set to true to display messages
int traceon=0;          // set to 1 to turn trace z80 on
                        // set by f2
int tracestartaddress=0;    // trace will only show results when the PC is within this range
int traceendaddress=0xFFFF; //  end address of the trace range


int usebiosmonitor=0;   // set to 1 to make use of the bios moitor

int scaledisplays=2;    // default scale display 

int calldisableSBROM=0; // used to count down to the disable of SBROM

// used to set the "jump on reset" links on the nascom 2
// DA N4 used to start SBROM on the N4
int JumpOnResetaddress=0;
int JumpOnResetaddressfixed=0; // this one is JumpOnResetaddress>>RAMPAGESHIFTBITS


/*
 *
 * end of globals
 *
 */


/*
 * define internal functions
 */
void setPortRemap(unsigned char value);

static void save_nascom(int start, int end, const char *name);
// static void reportdisplaymodes(void);
static int setdisassemblerrange(char * valuerange);

// called to refresh screens and the check for keyboard requests
//
int sim_delay(void)
{
    sim_action_t localaction = CONT;

    // update the status display
    status_display_refresh();
    // refresh both screens
    // update the nascom display
    nascom_display_refresh();
    // update the vfc display
    map80vfc_display_refresh();
    
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
    // global set to continue
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
 "\t{flags}\n"
 "\t\t-n           emulator Nascom 2, the default\n"
 "\t\t-v           emulate VFC boot for CPM\n"
 "\t\t-4           emulate the N4 \n"
 "\t\t-c <file>    Use this file as the N4 SD card image\n"
 "\t\t-m <file>    use <file> as monitor (default is rom/nassys3.rom)"
 "\t\t-i <file>    take serial port input from file (if tape led is on)\n"
 "\t\t-o <file>    send serial port output to file\n"
 "\t\t-f <configfile> load floppy drive - repeat for up to 4 drives\n"
 "\t\t-s factor    change the window sizes - default factor is 2\n"
 "\t\t-t           output a trace of the Z80 opcodes executed ( also F2 )\n"
 "\t\t-l start:end limits the trace process to with an address range\n"
 "\t\t-d <level>   debug level : 1 is verbose\n"
 "\t\t-x           use bios monitor when starting and stopped\n"
 "\t\tfiles        a list of nas files to load\n"

            ,progname);
}


int main(int argc, char **argv){

    int c;

    char *monitor;      // name of the monitor rom
    char *progname;     // name of program

    char *vfcromname;   // name of the vfc rom
    char *SBROMname;    // name for the SBROM DA N4
    
    char *floppy[4];
    int numberofFloppies = -1;

    char *sdcard = NULL;
    int SDCardPresent = 0;

    char firstcommand[]="E0";  // used as the first command if not using biosmonitor 

// fix DA moved roms to the roms folder
    monitor = "roms/nassys3.nas";
    vfcromname = "roms/vfcrom0.nas";   // name of the vfc rom based at zero
    SBROMname= "roms/z80_sboot_rom.nas"; // load the SBROM here DA N4
    sdcard="roms/nascom4_sdcard.img"; // default image for N4 sd card
    progname = argv[0];
    
    // will display the display modes avaiable
    //printf("display modes\n");
    //reportdisplaymodes();
    // it returns ? if invalid option having reported invalid option
    // do we need a multiple level for option d ?? TODO
    while ((c = getopt(argc, argv, "l:m:c:f:i:o:s:d:xnv4t")) != EOF) {
        
        // printf("Option %c\n",c);
        
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
        case 't':
            traceon=1;
            break;
        case 'n':
            emulator_mode='n';
            break;
        case 'v':
            emulator_mode='v';
            break;
        case '4':
            emulator_mode='4';
            break;
        case 'm':
            monitor = optarg;
            break;
        case 'd':   // was v now d for debug 
            // having different numbers but all can be active
            for (int cpos=0;cpos<strlen(optarg);cpos++){
                switch (optarg[cpos]){
                case '1':
                    verbose=1;
                    break;
                case '2':
                    nascompagedebug=1;
                    break;
                case '3':
                    chsclockcarddebug=1;
                    break;
                case '4':
                    N4portdebug=1;
                    break;
                case '5':
                    vfcdisplaydebug=1; 
                    break;
                case '6':
                    vfcfloppydebug=1;
                    break;
                case '7':
                    vfcfloppydisplaysectors=1;
                    break;
                default:
                    if (optarg[cpos] == '-') {
                        printf ("Think you forgot the value for -d ? \n");
                    }else {
                        printf("-d option value %s not recognised\n",optarg);
                    }
                    break;
                }
                
            }

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
            printf("unreconised option %c \n",optopt);
            usage(progname);
            exit (9);
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
        
    }

    // fix DA - added F1 triggers NMI

    puts("MAP80 Nascom, a Nascom 2 emulator version " VERSION "\n"
         "with MAP80 256 ram, MAP80 VFC display and floppy card,\n"
         "CHS clock card and Neal Crooks's N4 with SD card.");

    printf("**** Emulator now in ");
    switch (emulator_mode) {
    case 'n':
        printf("Nascom 2");
        break;
    case 'v':
        printf("VFC boot CPM");
        break;
    default:
        printf("N4");
        break;
    }
    printf(" mode ****\n");


    if (verbose){
        char cwd[FILENAME_MAX]; //create string buffer to hold path
        // DA Apr 2026 added to check return from getcwd
        char* ReturnPointer;
        ReturnPointer=getcwd( cwd, FILENAME_MAX );
        if (ReturnPointer == NULL ){
            printf("error getting current directory");
        }
        else {
            // used this to check that the returned pointer was the same address as my buffer
            // printf("ReturnPointer %p and cwd %p \n",ReturnPointer,cwd);
            printf("Current directory is '%s'\n",cwd);
        }
    }
    
    if (sdl_initialise()){
        // setup SDL
        fprintf(stderr,"failure to initialise SDL \n");
        exit(4);
    }   // setup SDL

    // sets the rampagetable entries to the first 64k of the ram space
    map80RamInitialise();


    //   printf("NascomMonVWram address %p \n",&NascomMonVWram);
    //   printf("NascomMonVWram screen address %p \n",&NascomMonVWram[0x800]);
    
    // set the positions of the various screens
    // both nascom and vfc screens are processed
    // technically only one should be seen 
        // use the standard Nascom display
        nascomdisplayxpos=NASCOM_DISPLAY_XPOS;
        nascomdisplayypos=NASCOM_DISPLAY_YPOS;
        // use the vfc display
        map80vfcdisplayxpos=MAP80VFC_DISPLAY_XPOS;
        map80vfcdisplayypos=MAP80VFC_DISPLAY_YPOS;
       
// status screen position
//        statusdisplayxpos=nascomdisplayxpos+NASCOM_DISPLAY_WIDTH+10;
//        statusdisplayypos=STATUS_DISPLAY_YPOS;OEF

//        statusdisplayxpos=map80vfcdisplayxpos+MAP80VFCDISPLAY_DISPLAY_WIDTH+10;
//        statusdisplayypos=STATUS_DISPLAY_YPOS;

        statusdisplayxpos=STATUS_DISPLAY_XPOS;
        statusdisplayypos=STATUS_DISPLAY_YPOS;


    // setup the status screen
    if (status_create_screen( (&statusdisplayram[0]) )){
        return 1;
    }
    // scale it to match others 
    status_display_change_size(scaledisplays);
    status_display_position(statusdisplayxpos, statusdisplayypos) ;

    int status_display_width=0;
    int status_display_height=0;
    int currentwidth=0;
    int currentheight=0;
    // get status disaply size so we can adjust the others
    status_GetWindowSize(&status_display_width,&status_display_height);

        // create nascom screen in it's own ram
        // screen is in the nascom ram space
        if (nascom_create_screen( (&NascomMonVWram[0x800]) )){
            return 1;
        }
        nascom_display_change_size(scaledisplays);
        nascomdisplayypos=statusdisplayypos+status_display_height+NASCOM_DISPLAY_YPOS+30;
        nascom_display_position(nascomdisplayxpos,nascomdisplayypos);
        nascom_GetWindowSize(&currentwidth,&currentheight);
        if (verbose){
            printf("current Nascom screen width: %d height: %d \n",currentwidth,currentheight);
       }

//        status_display_position(statusdisplayxpos, statusdisplayypos) ;
//    changed putting status display first 

        // create vfc screen in it's own ram
        if (map80vfc_create_screen( (&vfcdisplayram[0]) )){
         return 1;
        }
        map80vfc_display_change_size(scaledisplays);
        map80vfcdisplayypos=statusdisplayypos+status_display_height+MAP80VFC_DISPLAY_YPOS+30;
        map80vfc_display_position(map80vfcdisplayxpos,map80vfcdisplayypos);
        map80vfc_GetWindowSize(&currentwidth,&currentheight);
//        if (verbose){
//            printf("current Map80 screen width: %d height: %d \n",currentwidth,currentheight);
//        }
//        status_display_position(statusdisplayxpos, statusdisplayypos) ;


    //load_nascom(monitor);
    // load nas monitor into the first 2k of NascomMonVWram ram and ensure it is only 2048 bytes
    if (loadNASformatspecial(monitor, &NascomMonVWram[0], 2048 ) != 0 ){
        if (emulator_mode!='v'){ // if in cpm mode ignore error
            fprintf(stderr,"Failure in loading %s \n",monitor);
            exit (1);
        }
    }

    // load VFC Boot rom into the first 2k of vfcrom rom and ensure it is only 2048 bytes
    if (loadNASformatspecial(vfcromname, &vfcrom[0], 2048 ) != 0 ){
        if (emulator_mode=='v'){ // if not in cpm mode ignore error
            fprintf(stderr,"Failure in loading %s \n",vfcromname);
            exit (1);
        }
    }

    // load SBROM rom into own and ensure it is only 1024 bytes
    //if (loadNASformatspecial(SBROMname, &SBROM[0], 1024 ) != 0 ){
    if (loadNASformatspecial(SBROMname, &SBROM[0] , 1024 ) != 0 ){

        if (emulator_mode=='4'){ // if not in cpm mode ignore error
            fprintf(stderr,"Failure in loading %s \n",SBROMname);
            exit (1);
        }
    }



    for (; optind < argc; optind++){
        // load it into the first 64k of the standard memory 
        // The code now uses the RAM macro to load the bytes into the 64k as set by the MAP80 mapping
        // at this stage it should all be 64k of block 0
        // if it did try and write to NASSYS3 rom area 
        // the process recognises such rom stuff and writes to the underlying mapped memory using ramdefaultpagetable
        //
        int retval=loadNASformat(argv[optind]);
        if (retval){
                printf("Problem loading %s\n",argv[optind]);
        }
    }

    //if (SDCardPresent) {
        // DA N4 since I have set a default then mount it anyway :)
        SDMountDisk(sdcard);
    //+}


    // ensure all drives are reset
    resetalldrives();
    
    // now mount the drives requested
    for (int floppynumber=0;floppynumber<=numberofFloppies;floppynumber++){
        floppyMountDisk(floppynumber,floppy[floppynumber]);
    }
    
    
    displayfloppydetails();


// N4 changes - need to enable Nascom Rom via port control

    // clear the first command if we want to use the bios monitor.
    if (usebiosmonitor!=0){
        // blank out the first command.
        firstcommand[0]=0;
    }


    // DA N4 allow a jump on reset address
    // we will do this like N4
    // set stuff for first boot
    // does all the setup based on emulatioon type 
    // 0 means do a cold restart
    resetEmulator(0);


    MAP80nascomMonitor(firstcommand);

    // da n4 changed to only do this if not VFC (cpm) mode
    if (emulator_mode != 'v'){
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
            // TODO - add PIO and CTC controls 
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

        // added for N4 controls
        case 0x18:   //REMAP
            // put the value to Port_REMAP and check what must happen
            // need to be careful about SBROM disable
            // when we change REMAP this may impact VFC flag 
            // so if it has changed then need to call port 0xEC with 0 ?
            if (N4portdebug){
                fprintf(stdout, "Port_REMAP (18) changed from %02x to %02x\n", Port_REMAP_value, value);
            }
            setPortRemap(value);
            
            break;
        case 0x19:   //PROTECT
            // need to work out how
            if (N4portdebug){
                fprintf(stdout, "Port_PROTECT (19) changed from %02x to %02x\n", Port_PROTECT_value, value);
            }
            setPortProtect(value);
            break;
        case 0x1A:   //MWAITS
            if (N4portdebug){
                fprintf(stdout, "Port_MWAITS (1A) changed from %02x to %02x\n", Port_MWAITS_value, value);
            }
            Port_MWAITS_value=value;
            break;
        case 0x1B:   //PORPAGE
            if (N4portdebug){
                fprintf(stdout, "Port_PORPAGE (1B) changed from %02x to %02x\n", Port_PORPAGE_value, value);
            }
            Port_PORPAGE_value=value;
            break;
        case 0x1C:   //REASON
            if (N4portdebug){
                fprintf(stdout, "Port_REASON (1C) changed from %02x to %02x\n", Port_REASON_value, value);
            }
            Port_REASON_value=value;
            break;
        case 0x1D:   //SERCON
            if (N4portdebug){
                fprintf(stdout, "Port_SERCON (1D) changed from %02x to %02x\n", Port_SERCON_value, value);
            }
            // not doing anything at preset on serial speeds
            Port_SERCON_value=value;
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

       // added for N4 controls
        case 0x18:   //REMAP
            // put the value to Port_REMAP and check what must happen
            // need to be careful about SBROM disable
            retval=Port_REMAP_value;
            break;
        case 0x19:   //PROTECT
            // need to work out how
            retval=Port_PROTECT_value;
            break;
        case 0x1A:   //MWAITS
            retval=Port_MWAITS_value;
            break;
        case 0x1B:   //PORPAGE
            retval=Port_PORPAGE_value;
            break;
        case 0x1C:   //REASON
            retval=Port_REASON_value;
            break;
        case 0x1D:   //SERCON
            // not doing anything at preset on serial speeds
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

/*
 * PortPROTECT (0x19 ) 
 * Port: $19 (READ/WRITE)
   Name: PROTECT
   Notes: The reset value is UNDEFINED. The write-protect address regions correspond to the
   NASCOM 2 monitor ROM, EPROMs on the NASCOM 2 board and the NASCOM 2 BASIC ROM.
    * 
    7            Unused: write data ignored, read 0.
    6 PROTEF8K1: Write-protect 8Kbyte region starting from $E000
    5 PROTDF4K1: Write-protect 4Kbyte region starting from $D000
    4 PROTCF4K1: Write-protect 4Kbyte region starting from $C000
    3 PROTBF4K1: Write-protect 4Kbyte region starting from $B000
    2 PROTAF4K1: Write-protect 4Kbyte region starting from $A000
    1            Unused: write data ignored, read 0.
    0 PROT0F2K     1: Write-protect 2Kbyte region starting from $0000
    * 
 */
void setPortProtect(unsigned char value){
     
    // set the ramromtable using bit 4  ( 0x10 ) but dont change the rest
    // ?? TODO do we check if already set and onlyu change if needed
    // for now just set or unset them all 
    unsigned int setmask = (1 << 4); // set a bit in the mask;
    unsigned int unsetmask = ~(1 << 4); // clear a bit in the mask;

    if (value & 0x40){
        // set protect on  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xE000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<8;entry++){
            ramromtable[baseentry+entry]|=setmask;
        }
    }else {
        // set protect off  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xE000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<8;entry++){
            ramromtable[baseentry+entry]&=unsetmask;
        }
        
    }
    //5 PROTDF4K1: Write-protect 4Kbyte region starting from $D000
    if (value & 0x20){
        // set protect on  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xD000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<4;entry++){
            ramromtable[baseentry+entry]|=setmask;
        }
    }else {
        // set protect off  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xD000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<4;entry++){
            ramromtable[baseentry+entry]&=unsetmask;
        }
        
    }
    //4 PROTCF4K1: Write-protect 4Kbyte region starting from $C000
    if (value & 0x10){
        // set protect on  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xC000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<4;entry++){
            ramromtable[baseentry+entry]|=setmask;
        }
    }else {
        // set protect off  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xC000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<4;entry++){
            ramromtable[baseentry+entry]&=unsetmask;
        }
        
    }
    //3 PROTBF4K1: Write-protect 4Kbyte region starting from $B000
    if (value & 0x08){
        // set protect on  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xB000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<4;entry++){
            ramromtable[baseentry+entry]|=setmask;
        }
    }else {
        // set protect off  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xB000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<4;entry++){
            ramromtable[baseentry+entry]&=unsetmask;
        }
        
    }
    //2 PROTAF4K1: Write-protect 4Kbyte region starting from $A000
    if (value & 0x04){
        // set protect on  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xA000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<4;entry++){
            ramromtable[baseentry+entry]|=setmask;
        }
    }else {
        // set protect off  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0xA000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<4;entry++){
            ramromtable[baseentry+entry]&=unsetmask;
        }
        
    }
    //1            Unused: write data ignored, read 0.
    //0 PROT0F2K     1: Write-protect 2Kbyte region starting from $0000
    if (value & 0x01){
        // set protect on  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0x0000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<2;entry++){
            ramromtable[baseentry+entry]|=setmask;
        }
    }else {
        // set protect off  PROTEF8K1: Write-protect 8Kbyte region starting from $E000
        int baseentry = 0x0000>>RAMPAGESHIFTBITS;
        for (int entry=0;entry<2;entry++){
            ramromtable[baseentry+entry]&=unsetmask;
        }
        
    }
//    if (verbose){
//        displayRamTable();
//    }
    
    Port_PROTECT_value=value;
}


/*
 * To change the areas "mapped" in and out based on the N4 controls 
 * DA N4 
 * Reset only bit 2 is set to 1
    7
    6 CHARGEN0: VFC ROM/RAM regions behave normally.
        1: VFC RAM region occupies the whole 4Kbyte window
        of the VFC and provides write-only access to the character
        generator (see Programmable Character Generator on page 32)
    5   VFCAUTOMAP VFC autoboot. Determines effect of VFCMAP[1] (Port $EC). 
            This mimics the function of the L4 jumper on a MAP80 VFC board.
    4 NASWSRAM1: Enable NASCOM workspace RAM; 1Kbytes at $0C00.
    3 NASROM1: Enable NASCOM ROM monitor; 2Kbytes at $0000.
    2 SBROM1: Enable special boot ROM; 1Kbytes ar $1000. 
    1 NASVRAMHI
        0: NASCOM video RAM is decoded at $0800.
        1: NASCOM video RAM is decoded at $F800 (for NASCOM CP/M).
    0 NASVRAM1: Enable NASCOM video RAM; 1Kbytes
 */

void setPortRemap(unsigned char value){
    // need to save it before processing as port 0xEC will check it 
    // so keep current value and save new value into global variable
    // DA N4 - need to only disable / enable if entry changed from last time 
    unsigned char  previousValue=Port_REMAP_value;
    Port_REMAP_value= value;

    // NASVRAM control 
    if (value & 0x01) {
      // check on bit 2 to get address
      EnableNascomVideoRam(value & 0x02);
    }
    else {
        // only remove if the value has changed
        if ((value & 0x01) != (previousValue & 0x01)){
          DisableNascomVideoRam(value & 0x02);
        }
        // 0x02 determines the address of the nascomvideo
    }
 

   // SBROM control 
    if (value & 0x04) {
            EnableSBROM();
    } 
    else {
        // only do something if the value has changed
        if ((value & 0x04) != (previousValue & 0x04)){
           // needs care are needs to map out after next instruction
           // but only if it is mapped in ??? 
           calldisableSBROM=2; // when it gets to 1 rom will be disabled 
           // DisableSBROM();
        }
    }
    
    // NASROM control 
    if (value & 0x08) {
        EnableNascomMonitor();
    }
    else {
        // only do something if the value has changed
        if ((value & 0x08) != (previousValue & 0x08)){
            DisableNascomMonitor();
        }
    }
    
    // NASWRAM control 
    if (value & 0x10) {
        EnableNascomWorkRam();
    }
    else {
        // only do something if the value has changed
        if ((value & 0x10) != (previousValue & 0x10)){
            DisableNascomWorkRam();
        }
    }
    
    // VFCAUTO control 
    // sort of sets the "link 4" on the vfc card
    // determines the effect of the VFCMAP on port 0xEC
    // was originally set by the -b option but . . . . N4    
    // this is now read directly when needed 
    
    // if the value has changed  then call port EC
//    if ((value & 0x20) != (previousValue & 0x20)) {
        // this will either map in or out the VFC Rom
    if ((value & 0x20) ){
        //printf("VFC set to new mode %02X \n",value);
        out(0xEC,0);
        
    }
     
    // CHARGEN VFC control 
    // sort of sets the "link 4" on the vfc card
    // determines the effect of the VFCMAP on port 0xEC
    // was originally set by the -b option but . . . . N4
    
//    if (value & 0x40) {
//    }
//    if (verbose){
//        displayRamTable();
//    }
    Port_REMAP_value=value;

}

    
/*
 * With different setups the reset has become a bit more complex
 * so put all the details here so they were in one place
 * 
 * resetType is 0 for cold 1 for warm
 */
void resetEmulator(int resetType){

    // first reset all memory pages 
    resetMemoryPages();
    // sort of equivalent of  out(0xFE,0); // reset paging on MAP80RAM card

    // if emulating the N4 then need to do this 
    if (emulator_mode=='4'){
        JumpOnResetaddress=0x1000;  // set for SBROM 
        if (resetType==0){ // cold reset
            // this will reset PC to 0 need to reset hardware etc - moved from sdlevents 
            // should be done as resetMemoryPages - out(0xEC,0); // reset paging on MAP80VFC 
            out(0x1C,0x80 ); // REASON port set to say cold boot ( not sure about bit 6 never booted ??)
            out(0x18,0x04);// should EnableSBROM();
            out(0xEC,0); // switch out VFC stuff
            out(0xEF,0); // bring nascom screen back
        } else {
            out(0x1C,0x40 ); // REASON port set to say warm boot
            out(0x18,Port_REMAP_value|0x04);// should EnableSBROM() and leave rest alone;
            out(0xEC,0); // switch in VFC rom is needed (switch out the VFC screen memory)
        }
    } else if (emulator_mode=='v') {
        JumpOnResetaddress=0x0000;  // set for standard
        // this will reset PC to 0 need to reset hardware etc - moved from sdlevents 
        // part of resetMemoryPages - out(0xFE,0); // reset paging on MAP80RAM card
        out(0x18,0x20); // disable all nascom stuff and set vfc auto
        // ??? TODO check out(0xEC,0); // reset vfc rom if not done by above 
        out(0xEE,0);  // will select the VFC display as main and hide the nascom one
    } else { // assume nascom 2
        JumpOnResetaddress=0x0000;  // set for standard
        // this will reset PC to 0 need to reset hardware etc - moved from sdlevents 
        //part of resetMemoryPages - out(0xFE,0); // reset paging on MAP80RAM card
        //part of resetMemoryPages - out(0xEC,0); // reset paging on MAP80VFC
        // TODO do we need to remove SBROM 
        out(0x18,0x19);// should Enable nascom 2 stuff ;
        out(0xEF,0); // bring nascom screen to front and hide the VFC one
        out(0xEC,0); // switch in VFC rom is needed (switch out the VFC screen memory)
        
    }
    // set the jumponreset on all cases - but only used for N4 ?
    JumpOnResetaddressfixed=JumpOnResetaddress>>RAMPAGESHIFTBITS; // this one is JumpOnResetaddress>>RAMPAGESHIFTBITS
   // need the JumpOnResetaddressfixed to be set
   //  PC = 0;		// reset the emulator has to be done in main routine ???

}    



// end of code