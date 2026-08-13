/*
  simple monitor for MAP80 Nascom
   * 
   * This is not the NASSYS monitor 
   * It gets called from the map80Nascom modules after all standard stuff has been loaded
   * but before the Z80 simulator is fully started.
   * 
   * You can supply commands to run so it will start the emulator straight away TODO 
   * 
   * It also gets control when the emulator encounters a HALT instruction 
   * or F4 is pressed.
   * 
   * It has a number of simple commands that allow you to 
   *   display the memory the Z80 sees.
   *   disassemble the z80 memory
   *   output to (O) and query from (Q) the "ports"
   *   
   * 
  */


#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdbool.h>
#include <ctype.h>

#include "options.h"  //defines the options to usemap80RamIntialise
#include "simz80.h"
#include "map80nascom.h"
#include "map80ram.h"
#include "display.h"
#include "map80VFCfloppy.h"
#include "map80VFCdisplay.h"
#include "sdlevents.h"
#include "nasutils.h"   // define load program for .nas files
#include <SDL2/SDL.h>
#include "biosmonitor.h"
#include "utilities.h"
#include <stdio.h>
#include "disassemble.h"


int NumberofArgs=0;
int Args[10];

// edit the memory stores in the rampagetable 
// works like the Nascom M command
int editMemory(void){
    
    int bufferposition=0;
    int StartAddress=Args[0];
    int CurrentAddress;
    int argvalue;

    char inputdata[101];
    char * ReturnPointer;
    while (true){
        CurrentAddress=StartAddress;
        printf("%4.4X  %2.2X",CurrentAddress,RAM(CurrentAddress));
        // da apr 2026 - added checking the returned value in case over overflow :)        
        ReturnPointer=fgets( inputdata, 100, stdin );
        if (ReturnPointer == NULL ){
            printf("error getting stdin data");
        }
        else {
            // used this to check that the returned pointer was the same address as my buffer
            printf("ReturnPointer %p and cwd %p \n",ReturnPointer,inputdata);
            printf("input %s\n",inputdata);
        }
        
        while (true){
            argvalue=0;
            // check for white space at start and beteen values
            while (iswhitespace(inputdata[bufferposition])){
                bufferposition++;
            }
            // see if at end of string
            if ((inputdata[bufferposition] == 0x00) || (inputdata[bufferposition] == 0x0A) ) {
                StartAddress=CurrentAddress;
                break;
            }
            else if (inputdata[bufferposition] == '.') {
                return 0;
            }
            else if (inputdata[bufferposition] == ',') {
                RAM(CurrentAddress)=inputdata[bufferposition+1]&0xFF;
                bufferposition++;
                CurrentAddress++;
                
            }
            else if (inputdata[bufferposition] == ':') {
                StartAddress=CurrentAddress-1;
                break;
            }
            while (true){
                char tempchar = mytolower(inputdata[bufferposition]);
                if (tempchar<'0' || tempchar>'9'){
                    // not numeric
                    if (tempchar<'a' || tempchar>'f'){
                        // not alpha
                        break;
                    }
                    else {
                        // alpha character
                        argvalue = (argvalue<<4)+(tempchar-'a')+10; 
                    }
                }
                else {   
                        // number character
                        argvalue = (argvalue<<4)+(tempchar-'0'); 
                }
                bufferposition++;
            }
            // end of hex value found
            // add to array and inc counter
            //printf ("found %4.4X \n",argvalue);
            // store just the 16bit address
            // update value 
            RAM(CurrentAddress)=argvalue& 0xff;
            CurrentAddress++;

            if (inputdata[bufferposition] == 0x00){
                StartAddress=CurrentAddress;
                break;
            }
            if (inputdata[bufferposition] == 0x0A){
                StartAddress=CurrentAddress;
                break;
            }
            if (inputdata[bufferposition] != 0x20){
                printf("character %2.2X at position %d not space\n",inputdata[bufferposition],bufferposition);
                printf("Error?");
                
                break;
            }
            
            if (NumberofArgs > 10){
                // arg array full
                break;
            }

        }
        
        break;
        
    }

    return 0;
}


int getarguments(char * inputdata){

    int bufferposition=0;
    int argvalue=0;
    NumberofArgs=0;
    while (true){
        // check for white space at start and beteen values
        while (iswhitespace(inputdata[bufferposition])){
            bufferposition++;
        }

        if (inputdata[bufferposition] == 0x00){
            break;
        }

        argvalue=0;
        while (true){
            char tempchar = mytolower(inputdata[bufferposition]);
            if (tempchar<'0' || tempchar>'9'){
                // not numeric
                if (tempchar<'a' || tempchar>'f'){
                    // not alpha
                    break;
                }
                else {
                    // alpha character
                    argvalue = (argvalue<<4)+(tempchar-'a')+10; 
                }
            }
            else {   
                    // number character
                    argvalue = (argvalue<<4)+(tempchar-'0'); 
            }
            bufferposition++;
        }
        // end of hex value found
        // add to array and inc counter
        //printf ("found %4.4X \n",argvalue);
        // store just the 16bit address
        Args[NumberofArgs++]= argvalue & 0xFFFF;
        if (inputdata[bufferposition] == 0x00){
            break;
        }
        if (inputdata[bufferposition] == 0x0A){
            break;
        }
        if (inputdata[bufferposition] != 0x20){
            printf("character %2.2X at position %d not space\n",inputdata[bufferposition],bufferposition);
            return 1;
            break;
        }
        
        if (NumberofArgs > 10){
            // arg array full
            break;
        }

    }
    return 0;
}

void displayBytes(void){

    int StartAddress = Args[0];
    int EndAddress = StartAddress;
    int NumberofBytes=8;

    if (NumberofArgs>1){
        EndAddress=Args[1];
    }

    unsigned char asciiData[NumberofBytes+1];
    unsigned char asciichar;

    do{
        printf("%4.4X ", StartAddress );
        for (int count1=0;count1<NumberofBytes;count1++){
            printf("%2.2X ", RAM(StartAddress+count1) );
            asciichar = RAM(StartAddress+count1) & 0x7F;
            if ( ( asciichar >= 0x20 ) ){
                asciiData[count1]=asciichar;
            }
            else{
                asciiData[count1]='.';
            }
        }
        asciiData[NumberofBytes]=0;
        printf(" %s\n", asciiData);
        
        StartAddress+=NumberofBytes;
    } while(StartAddress<EndAddress);

    return;
    
}

// disassemble lines of code
void DoDisassemble(void){
    
    int StartAddress=Args[0];
    int EndAddress=StartAddress+1;
//    char disstr[100];
//    unsigned char disdata[5];
    int lencmd ;

    if (NumberofArgs>1){
        EndAddress = Args[1];
    }
    do {

        lencmd=disassembleprogram(StartAddress,stdout,0,0,0xFFFF,0,0,0,0,0);
        //fprintf(stdout,"%s\n",disstr);
        StartAddress+=lencmd;
    } while (StartAddress<EndAddress);
    
   
}




int MAP80nascomMonitor(char * FirstCommand){

// process user input
    FASTWORK Retval;
    int usebiosmonitorDefault=usebiosmonitor; // stored here so the shift F4 can trigger bios monitor
    // TODO may be a better way ???
    int doFirstCommand=0;
    char commandstr[101]="";
    char * ReturnPointer;
    // if a command was passed over then execute it first
    // by  setting doFirstCommand
    // this skips the request for input 
    // but does reset doFirstCommand so that the second time it will ask for a command 
    // the external variable usebiosmonitor is used to exit if we dont want to use the bios monitor
    if (strlen(FirstCommand)>0){
        strcpy(commandstr,FirstCommand);
        doFirstCommand=1;
    }
    // On starting simulator, refresh the screen 
    sim_delay();

    for(;;){

        if (doFirstCommand==0){
            printf( "Bios:");
            // da apr 2026 - added checking the returned value in case over overflow :)        
            ReturnPointer=fgets( commandstr, 100, stdin );
            if (ReturnPointer == NULL ){
                printf("error getting stdin data");
            }
            else {
                // used this to check that the returned pointer was the same address as my buffer
                // printf("ReturnPointer %p and cwd %p \n",ReturnPointer,commandstr);
//                 printf( "\nYou entered: ");
//                 puts( commandstr );
            }
            //    fgets( commandstr, 100, stdin );
        }
        else{
            doFirstCommand=0; // reset first command control
        }

        if (strlen(commandstr) > 0 ){
            if(commandstr[0] != ' ' ){
                //printf("Calling get agr\n");
                getarguments(&commandstr[1]);
                //printf("number of arguments %d args %4.4X %4.4X\n",NumberofArgs,Args[0],Args[1]);
                        
                switch (toupper(commandstr[0])){
            
                    case 'T':   // tabulate command
                        displayBytes();
                        break;
                    case 'O':   // output to port 
                        if (NumberofArgs<2){
                            printf("Needs port number and value\n");
                        }
                        else{
                            out(Args[0],Args[1]);
                        }
                        break;
                    case 'Q':   // query a port 
                        if (NumberofArgs<1){
                            printf("Needs port number \n");
                        }
                        else{
                            int result=in(Args[0]);
                            printf("%2.2X\n",result);
                        }
                        break;

                    case 'M':   // memory update
                        editMemory();

                        break;
                        
                    case 'R':   // trace command mode
                        if (traceon==1){
                            printf("Turning disassemble trace off\n");
                            traceon=0;
                        }
                        else{
                            printf("Turning disassemble trace on\n");
                            traceon=1;
                        }
                    case 'D':    // disassemble from address
                        DoDisassemble();
                        break;
                    case 'E':   // execute command
                        
                        if (NumberofArgs>0){
                            pc=Args[0]&0xFFFF;
                        }

                        fprintf(stdout, "Calling simz80 starting at %4.4X\n",pc);
                        puts("The following keys are supported:\n"
                             "\n"
                             "* F1 - Triggers an NMI \n"
                             "* F2 - does N4 style warm reset of the emulated Nascom\n"
                             "* F3 - resets the emulated Nascom\n"
                             "* F4 - exits the emulator (press shift to exit to Bios Monitor)\n"
                             "* F5 - resets the position of the serial input \n"
                             "* F6 - force serial input on\n"
                             "* F7 - Turns on/off disassembler trace\n"
                             "* F8 - toggles between stupidly fast and \"normal\" speed\n"
                             "* F9 - toggles between \"raw\" and \"natural\" keyboard emulation\n"
                             "* END - leaves a nascom screen dump in `screendump` \n"
                             "\n");
                           
                        Retval=simz80(pc, t_sim_delay, sim_delay,0);

                        // On return from simulator, refresh the screen one last
                        // time, in order to see any final output eg before a HALT
                        sim_delay();
                        // external global variable to say if we need to use the monitor 
                        // TODO remove it and handle it if E0 first command then return 
                        // maybe ???
                        if (usebiosmonitor==0){
                            return 0;
                        }
                        usebiosmonitor= usebiosmonitorDefault; // reset bios monitor status after shift f4
                        fprintf(stderr,"Emulator stopped at %4.4X \n",Retval&0xFFFF);
                        fprintf(stdout,"Ensure console window is selected \n");
                        fprintf(stdout,"Then enter the x commands to exit the Bios process\n");
                        fprintf(stdout,"or ? for help\n");
                        
                        break;
                    case 'S': 
                        // single step rom bios monitor
                        // displays and executes the next opcode ?
                        {   int oldtraceon=traceon;
                            // change program counter
                            if (NumberofArgs>0){
                                pc=Args[0]&0xFFFF;
                            }
                           // single step
                            traceon = 2;
                            Retval=simz80(pc, t_sim_delay, sim_delay,1);
                            // On return from simulator, refresh the screen one last
                            // time, in order to see any final output eg before a HALT
                            sim_delay();
                            traceon=oldtraceon;
                        }
                        break;
                        
                    case 'X':
                        return 0;
                        break;
                    case 'Z':
                        displayRamTable();
                        break;
                    case '?':
                        printf("The bios program commands are\n"
                               " D xxxx YYYY  disassemble code address xxxx to yyyy \n"
                               " E xxxx       to start Z80sim from address xxxx \n"
                               " O xx yy      output value yy on 'port' xx \n"
                               " Q xx         query input from 'port' xx \n"
                               " R            toggles the disassemble trace on/off \n"
                               " S xxxx       to single step from address xxxx or last stop address. \n"
                               " T xxxx yyyy  output memory from address xxxx to yyyy \n"
                               " X            to exit \n"
                               " Z            to display the current memory mapping\n"
                               );
                        break;
                    default:
                        printf("unknown command %s\n",commandstr);
                        break;
                    
                }
                // a BIOS command may cause the display to change, so update it
                sim_delay();
            }
        }
    }


}

