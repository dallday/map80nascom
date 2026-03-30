/*   floppy controller based on the MAP80 VFC card.
    
     * It does work for bothe polydos and cpm
     * Some of the coding  around the values returned for status (E0) and drive port (E4) 
     * may be a little bit uncertain
     * and the handeling of the busy/ready/data status bits might need some work.
     * 
     * Note: some functions are not implimented
     * Multi sector reads and writes are not handled - you will get an error
     * Read track and write track just reset the busy indicator.
     * This means that the cpm format and interrogate programs will not work.
     * 
     * 
     * Read address returns the same "sort of valid" sector header each time.
     * 

    David Allday January 2021

*/

#include <stdlib.h>            // std libraries
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "options.h"           //defines the options to use map80RamIntialise
#include "simz80.h"            // define all the z80 simulator
#include "map80VFCfloppy.h"    // define the floppy data stuff
#include "utilities.h"          // some useful bits of code
#include "map80nascom.h"
#include "statusdisplay.h"

// global variables - initial values set in options.
int vfcfloppydebug=VFCFLOPPYDEBUG;
int vfcfloppydisplaysectors=VFCFLOPPYDISPLAYSECTORS;

// status details used to set status register and status port return values
// ports 0xE0 and 0xE4
// Status port    INTRQ, Not READY and DRQ
static int floppyInteruptRequest = 0; // set to 1 to provide interrupt to computer ???
// floppyNotReady  done by setting floppyDelayReady 
//static int floppyNotReady = 1; // cleared when floppy is ready
static int floppyDelayReady=0;  // set to 2 to set the ready flag for x-1 calls

static int floppyDataRequest = 0; // set to 1 if data ready to read or data ready to be written to
static int floppyDelayForByteRequest = floppyDelayByteRequestBy; // set to delay the setting of the request data flag
                                   // once < 1 it won't do any requests
                                   // so floppyDelayByteRequestBy should be 1 or more

static int floppyDataLost = 0 ; // set to 1 if data lost during read or write
static int floppyBusy = 0;  // busy doing command when 1
static int SetBusyForTime=floppyDelayResetBusy;   // if set > 1 then triggers busy status for x calls to get status

static int floppyHeadLoaded = 0; // set to 1 when the heads are "locked and loaded"
static int floppyCRCError=0;     // set to 1 if there is a crc error
static int floppySeekError = 0;  // set to 1 when seek error occured

static int floppyRecordNotFound = 0; // set to 1 if record not found
static int floppyWriteFailWriteProtected = 0; // set to 1 if write failed because disc write protected



// There are 2 separate values for track positions:
// - what the 1797 chip thinks the track number is: floppyTrackRegister
// - where the real head is on that floppy: floppyDrives[].track
// These may or may not match, especially after switching drives
//
// registers inside the 1797 floppy control chip
// (the status register is dynamically built on each call, from the individual status bits above)
static unsigned int floppyTrackRegister;
static unsigned int floppySectorRegister;
static unsigned int floppyDataRegister;
// floppyTrackRegister, floppySectorRegister, floppySide and floppyActiveDrive are globals representing 1797 state and used
// by the readASector/writeASector routines.
static unsigned int floppySide = 0;         // set to side by type II and III commands
static int floppyActiveDrive = 0;           // assume drive 0 is selected by default ?

// TODO remove position of the head on tracks
//unsigned int floppyPhysicalTrackPosition;

// floppyDrives hold the details of the images attached to them
// including the geometry    sprintf(strBuffer,"Tape position: %c input:%06ld  output:%06ld ", " *"[tape_led] , tape_in_pos, tape_out_pos ); 

// and current track position for that drive
static FLOPPYDRIVEINFO floppyDrives[NUMBEROFDRIVES];   // drives 0 to 4

static int floppyMaxTrackNumber=79; // tracks 0 to 79 - the hardware device only has 80 tracks????

static unsigned int floppyStepDirection=1;  // set to seek direction of last call
                                     // default to step in is away from track 0

static unsigned char floppyBuffer[FLOPPYMAXSECTORSIZE];  // buffer to hold latest sector
static unsigned int floppyBufferPosition=0;              // position in buffer for read or write
static unsigned int floppyBufferUsed=0;                  // how many bytes are in buffer

// the last commands recieved
static int floppyPreviousCommand=0;
static int floppyCurrentCommand=0;

// 
static int readaddresssector=-1;


// functions - internal
// port write
static int floppySetCommand(unsigned int value); // call to set command register
static int floppySetTrack(unsigned int value); // call to set track register
static int floppySetSector(unsigned int value); // call to set sector register
static int floppySetData(unsigned int value);   // set the data port
static int floppySetDrive(unsigned int value); // call to set active drive
// port read
static int floppyReadStatus(); // call to read the status register
static int floppyReadTrack(); // call to read Track register
static int floppyReadSector(); // call to read Sector register
static int floppyReadData(); // read data register
static int floppyReadDrivePort(); // call to read Drive port ( DRQ, INTRQ and READY )
// general
//void checkIfDiskLoaded();
//void setTypeIStatusRegister();
static void clearStatusIndcators();
static void readASector(unsigned int command);
static void writeASector(void);
static void readAddress(unsigned int command);
static void showFloppySector();
static void displayBuffer(unsigned char buffer[], int length );
static long int floppyFindOffset(int DriveNumber,int Track, int discSide, int Sector);
static int getSideFromCommand(unsigned int command);
static void displayDetails(char * message);
static void displayCommand(unsigned int command);
static void printdiscimageproperties(int drive);
static void dostep(int value);
static void formattrack(void);
static void displayfloppystatus(void);
static int getFloppyReadyFlag(void);
static int getsectorsizeLSBvalue(int sectorLengthFlag, unsigned int sectorsize);


// display floppy status - when motor on / off
static void displayfloppystatus(void){

    char strBuffer[]="  ";
    char drivestatus=0;
    
    for (int driveno=0;driveno<4;driveno++){
        if (floppyActiveDrive==driveno){
            drivestatus=0xB9;
        }
        else {
            drivestatus=0xB8;
        }
        strBuffer[0]=drivestatus;
        status_display_show_chars_full(strBuffer,2,(driveno*2)+1,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_RED,STATUS_BLACK);
        
    }
   
}
static void displayfloppyposition(){

    char strBuffer[250];
    
    
    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4)){
        sprintf(strBuffer,"Track %2d Head %d Sector %2d      ",floppyDrives[floppyActiveDrive].track,floppySide,floppyReadSector()); 
        status_display_show_chars_full(strBuffer,5,(floppyActiveDrive*2)+2,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_YELLOW,STATUS_BLACK);
        //printf("Floppy %d %s\n",floppyActiveDrive,strBuffer);
    }
   
}



// display floppy details on the status screen
void displayfloppydetails(){
    
    char strBuffer[250];
    char drivestatus=0;
    char filename[100];
    int linenumber=1;
    status_display_show_chars_full("Drives",0,0,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_WHITE,STATUS_BLACK);
    for (int driveno=0;driveno<4;driveno++){
        if (floppyDrives[driveno].fileNamePointer == NULL ){
            drivestatus=0xB8;
            filename[0]=0;
        }
        else {
            drivestatus=0xB8;
            int maxfilenamesize=20;
            copystringtobuffer(filename,floppyDrives[driveno].fileNamePointer,maxfilenamesize);
/*            if ((strlen(floppyDrives[driveno].fileNamePointer) > maxfilenamesize)){
                strncpy(filename,floppyDrives[driveno].fileNamePointer,maxfilenamesize);
                filename[maxfilenamesize]='.';
                filename[maxfilenamesize+1]='.';
                filename[maxfilenamesize+2]='.';
                filename[maxfilenamesize+3]=0;
            }
            else {
                strcpy(filename,floppyDrives[driveno].fileNamePointer);
            }
*/
        }
        strBuffer[0]=driveno+0x30;
        strBuffer[1]=' ';
        strBuffer[2]=0;
        status_display_show_chars_full(strBuffer,0,linenumber,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_ORANGE,STATUS_BLACK);
        strBuffer[0]=drivestatus;
        status_display_show_chars_full(strBuffer,2,linenumber,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_RED,STATUS_BLACK);
        // sprintf(strBuffer,"%d "%c %s ", driveno, drivestatus, filename ); 
        status_display_show_chars_full(filename,4,linenumber,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_YELLOW,STATUS_BLACK);
        linenumber+=2;
        
    }
    
    
    
    
}

// null the filenames of all drives 
void resetalldrives(){
    // clear all floppy drives 
    for (int driveno=0;driveno<4;driveno++){
        floppyDrives[driveno].fileNamePointer=NULL;
    }
}



// mount a floppy drive
int floppyMountDisk(unsigned int drive, char *configfilename){

// read the parameters for a floppy disk
// each disk has it's own config file that also tells up the file name
// drive number is set by main program as it reads the config files

    unsigned int setdrive=drive;

    int len;
    char * heap_string;

    FILE *filep;

    char keyword[255];
    char paramdata[255];

    int keywordpos=0;
    int paramdatapos=0;

    char ch=0;

    char buffer[255];
    char *bufferpointer=NULL;
    int bufferposition=0;

    int filelinenumber=0;

    long int paramValue=0;

    int notimplemented=0;   // set to 1 if the option has not been implemented

    if (drive > 3){
        fprintf(stdout, "Mount Floppy drive - Drive number %d too big for '%s', ignoring it. \n",drive, configfilename);
        return -1;
    }

    //    // dummy to start with - this is neals polydos 2 disc interleaved image
    //   char filename[]= "disks/TESTPD.BIN";

    /*

    from gotek image config
    cyls = 35
    heads = 2
    secs = 18
    bps = 256
    id=0
    mode=mfm
    iam = no
    #   interleaved:   Interleaved cylinder ordering: c0s0, c0s1, c1s0, c1s1, ...
    file-layout = interleaved

    current program assume files are interleaved

    */

    // set the default values

    // free the space used for the original file name
    if ( floppyDrives[setdrive].fileNamePointer != NULL ){
        if (vfcfloppydebug){
            printf("Mount Floppy drive - releasing memory\n");
        }
        free(floppyDrives[setdrive].fileNamePointer );
        // say no image loaded
        floppyDrives[setdrive].fileNamePointer = NULL;
    }

    // these are the polydos basic settings
    floppyDrives[setdrive].fileNamePointer = NULL;
    floppyDrives[setdrive].numberOfHeads=1;    // number of heads / sides
    floppyDrives[setdrive].numberOfTracks=35;    // number of tracks
    floppyDrives[setdrive].numberOfSectors=18;  // number of Sectors
    floppyDrives[setdrive].firstSectorNumber=0;
    floppyDrives[setdrive].interleaved=1;       // say track interleaved
    floppyDrives[setdrive].sidesswapped=0;       // not sided swapped
    floppyDrives[setdrive].reverseside0=0;      // side 0 not reversed
    floppyDrives[setdrive].reverseside1=0;      // side 1 not reversed
    floppyDrives[setdrive].writeProtect=0;      // write protect status off
    floppyDrives[setdrive].sizeOfSector = 256;   // size of each sector
    floppyDrives[setdrive].track=0;             // current track


    // open the config file
    filep = fopen(configfilename, "r");
    if ( filep == NULL) {
        fprintf(stdout, "Mount Floppy drive - config file '%s' failed to open.\n", configfilename);
        perror(configfilename);
        return 1;
    }

    // read 250 byte lines into buffer
    // allow extra byte for the added 0x00
    // returns NULL if end of file reached
    while ( (bufferpointer=fgets(buffer,250,filep))!=NULL){
       // process a line
        //fprintf(stdout, "Mount Floppy drive - config file '%s', line %d is '%s'. \n", configfilename,filelinenumber,buffer);
        filelinenumber++;
        if (strlen(buffer) >249 ){
            fprintf(stdout, "Mount Floppy drive - floppy config file '%s', line %d is too long - ignoring. \n", configfilename,filelinenumber);
            // skip to end of line in the file
            ch = buffer[249];
            while (ch != -1 && ch != '\n'){
                ch = fgetc(filep);
                printf("Mount Floppy drive - extras %02X \n",ch);
            }
            // end of file exit
            if (ch == -1){
                break;
            }
            continue;
        }
        // check for white space at start
        bufferposition=0;
        while (iswhitespace(buffer[bufferposition])){
            bufferposition++;
        }
        // check if end of line
        if (buffer[bufferposition] < 0x20 ){
            continue;
        }
        // check for comment
        if (buffer[bufferposition]=='#'){
            // comment found - ignore line
            continue;
        }
        keywordpos=0;
        // search for end of keyword - there will be a 0x00 at end of buffer
        while (!( iswhitespace(buffer[bufferposition]) || buffer[bufferposition]=='=' || buffer[bufferposition]<0x20) ) {
            keyword[keywordpos] = buffer[bufferposition];
            bufferposition++;
            keywordpos++;
        }
        // set end of string marker
        keyword[keywordpos]=0x00;
        //printf("Mount Floppy drive - config line %d keyword='%s' \n",filelinenumber, keyword);
        // again skip any white spaces
        while (iswhitespace(buffer[bufferposition])){
            bufferposition++;
        }

        if (buffer[bufferposition]=='='){
            bufferposition++;
        }
        else{
            fprintf(stdout, "Mount Floppy drive - config file '%s', line %d no = found - ignoring. \n", configfilename,filelinenumber);
            continue;
        }
        // again skip any white spaces
        while (iswhitespace(buffer[bufferposition])){
            bufferposition++;
        }

        paramdatapos=0;
        // search for end of keyword - there will be a 0x00 at end of buffer
        // process until either 0 or lf or cr or # found
        // while (!( iswhitespace(buffer[bufferposition]) || buffer[bufferposition]<0x20) ) {
        while (!( buffer[bufferposition]<0x20 || buffer[bufferposition]=='#') ) {
            paramdata[paramdatapos] = buffer[bufferposition];
            bufferposition++;
            paramdatapos++;
        }
        // set end of string marker
        paramdata[paramdatapos]=0x00;

        //printf("Mount Floppy drive - config line %d keyword='%s' data='%s' \n",filelinenumber, keyword,paramdata);

        notimplemented=0;

        // get keyword and data see if valid
        if(strcmp(keyword,"tracks")==0){
            // not using this keyword
            notimplemented=1;
        }
        else if(strcmp(keyword,"cyls")==0){
            // number of cylinders ( or tracks to you and me )
            paramValue=strtoll(paramdata,NULL,10);
            floppyDrives[setdrive].numberOfTracks=paramValue;
        }
        else if(strcmp(keyword,"heads")==0){
            // number of heads either 1 or 2
            paramValue=strtoll(paramdata,NULL,10);

            floppyDrives[setdrive].numberOfHeads=paramValue;
        }
        else if(strcmp(keyword,"secs")==0){
            // number of sectors per cylinder/track per side
            paramValue=strtoll(paramdata,NULL,10);

            floppyDrives[setdrive].numberOfSectors=paramValue;

        }
        else if(strcmp(keyword,"bps")==0){
            // number of bytes in a sector
            paramValue=strtoll(paramdata,NULL,10);

            floppyDrives[setdrive].sizeOfSector=paramValue;

        }
        else if(strcmp(keyword,"id")==0){
            // the number of the first sector on the track
            paramValue=strtoll(paramdata,NULL,10);
            floppyDrives[setdrive].firstSectorNumber=paramValue;

        }
        else if(strcmp(keyword,"h")==0){
            // not using this keyword
            notimplemented=1;
        }
        else if(strcmp(keyword,"mode")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"interleave")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"cskew")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"hskew")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"rpm")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"gap3")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"gap4a")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"iam")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"rate")==0){
            // not using this keyword
            notimplemented=1;

        }
        else if(strcmp(keyword,"write-protect")==0){
            // set disc write protected
            floppyDrives[setdrive].writeProtect = ((paramdata[0] == 'Y') || (paramdata[0] == 'y'));
            printf("Floppy write protect %d\n",floppyDrives[setdrive].writeProtect);
        }
        else if(strcmp(keyword,"file-layout")==0){
            // not using this keyword
            char *parampointer1, *parampointer2;
            for (parampointer1 = paramdata; *parampointer1 != '\0'; parampointer1 = parampointer2) {
                for (parampointer2 = parampointer1; *parampointer2 && *parampointer2 != ','; parampointer2++)
                    continue;
                if (*parampointer2 == ','){
                    *parampointer2++ = '\0'; // set end of string marker
                }
                // now see if we recognise the parameter value
                if (strcmp(parampointer1, "reverse-side0")==0) {
                    floppyDrives[setdrive].reverseside0=1;
                }
                else if (strcmp(parampointer1, "reverse-side1")==0) {
                    floppyDrives[setdrive].reverseside1=1;
                }
                else if (!strcmp(parampointer1, "interleaved")) {
                    floppyDrives[setdrive].interleaved=1;
                }
                else if (!strcmp(parampointer1, "sequential")) {
                    // set to say not interleaved == sequential
                    floppyDrives[setdrive].interleaved=0;
                }
                else if (!strcmp(parampointer1, "sides-swapped")) {
                    floppyDrives[setdrive].sidesswapped=1;
                }
                else {
                    fprintf(stdout, "Mount Floppy drive - config file '%s', line %d - unrecognised file-layout '%s'\n", configfilename,filelinenumber,parampointer1);
                }
            }

        }
        else if(strcmp(keyword,"imagefilename")==0){
            // set the file name of the image file
            len = strlen( paramdata );
            heap_string = malloc( len + 1 );
            strcpy( heap_string, paramdata );
            floppyDrives[setdrive].fileNamePointer = heap_string;
            //printf("found image name %s\n",paramdata);

        }
        else {
            fprintf(stdout,"Mount Floppy drive - config line %d keyword='%s' unknown - ignored \n",filelinenumber, keyword);
        }

        if (notimplemented==1){
            if (verbose){
                printf("Mount Floppy drive - config line %d keyword='%s' not yet implemented\n",filelinenumber, keyword);
            }
        }
    }

    fclose(filep); // close the config file

    if (floppyDrives[setdrive].fileNamePointer != NULL){

        filep = fopen(floppyDrives[setdrive].fileNamePointer, "r+b");
        if ( filep == NULL) {
            fprintf(stdout, "Mount Floppy drive - cannot find image file '%s'. \n", floppyDrives[setdrive].fileNamePointer);
            perror(floppyDrives[setdrive].fileNamePointer);
        }
        if ( filep != NULL) {
            fclose(filep);
        }
    }

    printf("Mount Floppy drive %d, config file '%s', image file '%s' \n",
                setdrive,configfilename,(floppyDrives[setdrive].fileNamePointer==NULL? "null":floppyDrives[setdrive].fileNamePointer ));
    if (verbose){

        printdiscimageproperties(setdrive);
        
    }

    return 0;
}



// handle all calls to the MAP80 VFC output ports
void outPortFloppy(unsigned int port, unsigned int value){

    printf("port out %2.2X %2.2X\n",port,value);

    switch (port) {
    case 0xE0:
        // P 0xE0 command write
        floppySetCommand(value);
        break;

    case 0xE1:
        // P 0xE1 track register
        floppySetTrack(value);
        break;

    case 0xE2:
        // P 0xE2 Sector register
        floppySetSector(value);
        break;

    case 0xE3:
        // P 0xE3 Data Register
        floppySetData(value);
        break;

    case 0xE4:
        // P 0xE4 Drive Port
        // drop into 0xE5
    case 0xE5:
        // P 0xE5 Drive Port - as 0xE4
        floppySetDrive(value);
        break;
    default:
        fprintf(stdout,"MAP80 VFC unhandled write to port %2.2X value %2.2X \n",port,value);
    }

}

void printdiscimageproperties(int driveNumber){

    printf("Image details for drive  %2.2X \n",driveNumber);
    printf("   Image file [%s]\n",floppyDrives[driveNumber].fileNamePointer);
    printf("   Number of Heads   [%d]\n",floppyDrives[driveNumber].numberOfHeads);
    printf("   Number of tracks  [%d]\n",floppyDrives[driveNumber].numberOfTracks);
    printf("   Number of Sectors [%d]\n",floppyDrives[driveNumber].numberOfSectors);
    printf("   First Sector      [%d]\n",floppyDrives[driveNumber].firstSectorNumber);
    printf("   Interleaved       [%d]\n",floppyDrives[driveNumber].interleaved);
    printf("   Size of Sector    [%d]\n",floppyDrives[driveNumber].sizeOfSector);


/*
    floppyDrives[driveNumber].sidesswapped=0;       // not sided swapped
    floppyDrives[driveNumber].reverseside0=0;      // side 0 not reversed
    floppyDrives[driveNumber].reverseside1=0;      // side 1 not reversed
    floppyDrives[driveNumber].writeProtect=0;      // write protect status off
    floppyDrives[driveNumber].track=0;             // current track
*/
    printf("\n");

}



// handle any read ports for the MAP80VFC Floppy controller
int inPortFloppy(unsigned int port){

int retval=0xFF;

    switch (port) {
    case 0xE0:
        // P 0xE0 command write
        retval=floppyReadStatus();
        break;

    case 0xE1:
        // P 0xE1 track register
        retval=floppyReadTrack();
        break;

    case 0xE2:
        // P 0xE2 Sector register
        retval=floppyReadSector();
        break;

    case 0xE3:
        // P 0xE3 Data Register
        retval=floppyReadData();
        break;

    case 0xE4:
        // P 0xE4 Drive Port
        // drop into 0xE5
    case 0xE5:
        // P 0xE5 Drive Port - as 0xE4
        retval=floppyReadDrivePort();
        break;
    default:
        fprintf(stdout,"MAP80 VFC unhandled read on port [%2X] \n",port);
    }

    printf("port in [%2X] returned [%2X]\n",port,retval);

    return retval;


}

// ********** internal functions from here on **********8

static void displayDetails(char * message){

    char CurState[]="Off";

    if (floppyActiveDrive> -1) {
        // floppy dirve number set
        if (floppyDrives[floppyActiveDrive].fileNamePointer ==NULL) {
            // but not loaded
            strcpy(CurState,"Nul");
        }
        else {
            // with a file
            strcpy(CurState,"On ");
        }

    }
    // display the current disk details after each call

    fprintf(stdout,"%s "
                  "TrackReg   %2.2X  "
                  "SectorReg   %2.2X  "
                  "Side   %2.2X  "
                  "DataReg   %2.2X  "
                  "drive  %2.2X  "
                  "pointer %s  \n",
                          message,
                          floppyTrackRegister,
                          floppySectorRegister,
                          floppySide,
                          floppyDataRegister,
                          floppyActiveDrive,
                          CurState);

}

//
// display what the command to the disk controller was
//
static void displayCommand(unsigned int command){
    // set side - only neeed for TYPE II and III commands
    int diskside =  getSideFromCommand(command);

    fprintf(stdout,"Command %2.2X ", command);
    switch ( command & 0xF0 )
    {
        case floppyCmdRestore:
            fprintf(stdout, "Restore");
            break;

        case floppyCmdSeek:
            fprintf(stdout, "Seek");
            break;

        case floppyCmdStep:
            fprintf(stdout, "Step");
            break;
        case floppyCmdStepTrackUpdate:
            fprintf(stdout, "StepTU");
            break;
        case floppyCmdStepin:
            fprintf(stdout, "StepIn");
            break;
        case floppyCmdStepinTrackUpdate:
            fprintf(stdout, "StepInTU");
            break;
        case floppyCmdStepout:
            fprintf(stdout, "StepOut");
            break;
        case floppyCmdStepOutTrackUpdate:
            fprintf(stdout, "StepOutTU");
            break;
        // type 2 commands
        case floppyCmdReadSector:
            fprintf(stdout, "ReadSector");
            fprintf(stdout, " side %d",diskside);
            break;
        case floppyCmdReadSectorMulti:
            fprintf(stdout, "ReadSectorMulti");
            fprintf(stdout, " side %d",diskside);
            break;
        case floppyCmdWriteSector:
            fprintf(stdout, "WriteSector");
            fprintf(stdout, " side %d",diskside);
            break;
        case floppyCmdWriteSectorMulti:
            fprintf(stdout, "WriteSectorMulti");
            fprintf(stdout, " side %d",diskside);
            break;
       // type 3 commands
        case floppyCmdReadAddress:
            fprintf(stdout, "ReadAddress");
            fprintf(stdout, " side %d",diskside);
            break;
        case floppyCmdReadTrack:
            fprintf(stdout, "ReadTrack");
            fprintf(stdout, " side %d",diskside);
            break;
        case floppyCmdWriteTrack:
            fprintf(stdout, "WriteTrack");
            fprintf(stdout, " side %d",diskside);
            break;
        // type 4 command
        case floppyCmdForceInterupt:
            fprintf(stdout, "ForceInterupt");
            break;
        default:
            // should not happen but . . . .
            fprintf(stdout, " unknown command  %2.2X ",command );
    }
    fprintf(stdout,"\n");

}

// the side being used is part of the TYPE II command
// use and on the bit to find out which side
static int getSideFromCommand(unsigned int command){
        // set side
        if (command & floppyCmdSelectSide ){
            return 1;
        }
        // if not set return 1
        return 0;
}

// output to the command port E0
static int floppySetCommand(unsigned int value){ // call to set command register

    // a long bit of code . . . . .

    if (vfcfloppydebug){
        displayCommand(value);
    }

    // first check id type 4 command
    if ( (value & 0xF0) == floppyCmdForceInterupt ){
            // only one that can be accepted when busy
            // this stops any current operations - i.e. read sector
            // if we were not busy then set status as if type 1
            // else leave alone  ??
            // busy only set for a time ??
            // if (SetBusyForTime < 1 ){
            if (floppyBusy == 0){
                // reset status register
                clearStatusIndcators();
            }
            
            // say command complete - set interrupt bit
            floppyDelayReady = 2;   // delay ready and not busy for 1 calls
            SetBusyForTime = 2;

    }
    else if (!floppyBusy) {
        // not busy so we can process the other comamnds
        // only the first 4 byte give the command
        // process the command request
        // the rest of the bits control extra functions like which side for read

        floppyCurrentCommand = value;
        // command received and being done
        // say busy
        floppyBusy = 1;
        // clear the other flags
        clearStatusIndcators();

        // is it a type I command is 00 to 7F
        if ((value & 0x80) == 0 ){

            floppyBusy = 1;

            switch ( value & 0xF0 ){
                case floppyCmdRestore:
                    // restore command - reset drive to track 0 both in the drive and the track register
                    // should do this when controller is reset
                    // set the status register to show we are at track 0
                    floppyStepDirection = 1; // next seek goes up in track numbers
                    floppyTrackRegister = 0;
                    floppySectorRegister = 0;
                    // reset the physical track position for active drive
                    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ) {
                        floppyDrives[floppyActiveDrive].track=floppyTrackRegister;
                    }

                    break;

                case floppyCmdSeek:
                    // seek a track
                    // track number should be in the data register
                    // so set the track register to the data register
                    // assuming it is within range.
                    // TODO - check if disc active???

                    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ) {
                        // decide how we would get to the new track
                        // and set direction
                        if (floppyTrackRegister>floppyDataRegister){
                            // we need to step out towards track 0
                            floppyStepDirection=-1;
                        }
                        if (floppyTrackRegister<floppyDataRegister){
                            // we need to step in away from track 0
                            floppyStepDirection=1;
                        }

                        // track is withing range
                        // set seek direction
                        floppyStepDirection = 1; // assume step out
                        if ( floppyDataRegister < floppyTrackRegister ){
                            // nope we are stepping in to get tothe track
                            floppyStepDirection = -1;
                        }
                        // so set track number for the
                        // TrackRegister and current floppy drive
                        // TODO not sure if we should set Physical track position
                        floppyTrackRegister = floppyDataRegister;
                        floppyDrives[floppyActiveDrive].track = floppyTrackRegister;
                        if (vfcfloppydebug){
                            printf("Track Register set to %2.2X\n",floppyTrackRegister);
                        }
                    }
                    else {
                        // say seek error
                        floppySeekError = 1;
                    }
                    floppyInteruptRequest = 1; // command completed

                    break;

                case floppyCmdStep:
                    // step in the direction previously used
                    // fall into the floppyCmdStepTrackUpdate
                case floppyCmdStepTrackUpdate:
                    // step in the direction previously used
                    // and update track register
                    /*
                    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ) {
                        // cannot step out any futher as at track 0
                        if (((floppyDrives[floppyActiveDrive].track > 0) && (floppyStepDirection < 1))
                           || ((floppyDrives[floppyActiveDrive].track < floppyMaxTrackNumber) && (floppyStepDirection > 0))) {
                            // update track position
                            floppyDrives[floppyActiveDrive].track += floppyStepDirection;
                            if  ( value & 0x10 ){
                               // update track register
                               floppyTrackRegister=floppyDrives[floppyActiveDrive].track;
                            }
                        }
                    }
                     */
                    dostep(value);  // do the step
                    SetBusyForTime=floppyDelayResetBusy;     // set busy flag for x-1 calls to status - then set floppyInteruptRequest
                    break;
                case floppyCmdStepin:
                    // step in 1 track - that is away from track 0
                    // fall through to the floppyCmdStepinTrackUpdate code
                case floppyCmdStepinTrackUpdate:
                    // step in 1 track
                    // and update track register
                    // reset status register
                    floppyStepDirection = 1;
                    dostep(value);  // do the step
/*
                    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ) {
                        if ( (floppyDrives[floppyActiveDrive].track < floppyMaxTrackNumber) && (floppyStepDirection > 0)){
                            // update track position
                            floppyDrives[floppyActiveDrive].track += floppyStepDirection;
                            if  ( value & 0x10 ){
                                // update track register
                                floppyTrackRegister=floppyDrives[floppyActiveDrive].track;
                            }
                        }
                    }
                     */
                    SetBusyForTime=floppyDelayResetBusy;     // set busy flag for x-1 calls to status then set floppyInteruptRequest
                    break;
                case floppyCmdStepout:
                    // step out 1 track  - that is towards track 0
                    // fall through to the floppyCmdStepOutTrackUpdate code
                case floppyCmdStepOutTrackUpdate:
                    // step out 1 track
                    // and update track register
                    floppyStepDirection = -1;
                    dostep(value);  // do the step
/*
                    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ) {
                        printf("Step out called - track is %2.2X direction %d\n",floppyDrives[floppyActiveDrive].track,floppyStepDirection);
                        // if ((floppyDrives[floppyActiveDrive].track > 0) && (floppyStepDirection < 1)){
                        if ((floppyDrives[floppyActiveDrive].track > 0) ){
                            // update track position
                            floppyDrives[floppyActiveDrive].track += floppyStepDirection;
                            printf("new track is %2.2X\n",floppyDrives[floppyActiveDrive].track);
                            if  ( value & 0x10 ){
                                // update track register
                                floppyTrackRegister=floppyDrives[floppyActiveDrive].track;
                            }

                        }
                    }
*/
                    // say that the head is loaded
                    SetBusyForTime=floppyDelayResetBusy;     // set busy flag for x-1 calls to status then set floppyInteruptRequest

                    break;
                default:
                    // should not happen but . . . .
                    fprintf(stdout, "floppy controller received unknown Type I command  %2.2X , ignoring it. \n", value);

            } // end of switch

            if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ){
                // if bit 3 set the mount the heads
                if ( (value & floppyCmdLoadHead ) == floppyCmdLoadHead ){
                    // set floppy head to loaded
                    floppyHeadLoaded = 1 ;
                    //fprintf(stdout, "floppy head loaded  %2.2X ,  %2.2X  \n", value,(value & floppyCmdLoadHead));
                }
                else {
                    // set floppy head to unloaded
                    floppyHeadLoaded = 0 ;
                }
            }
            // say command complete - set interrupt bit
            floppyBusy = 0;   // no longer busy

        } // end of if
        else{
            // type II III  commands
            // set side
            if (value & floppyCmdSelectSide ){
                floppySide=1;
            }
            else {
                floppySide=0;
            }
            // type 2 commands
            switch ( value & 0xF0 ){
                case floppyCmdReadSector:
                    // read a sector from the disk
                    readASector(value);
                    floppyBusy = 0;   // no longer busy
                    break;
                case floppyCmdReadSectorMulti:
                    // not doing this at present
                    floppyInteruptRequest = 1; // set interrupt
                    floppyRecordNotFound = 1; // say not found as error
                    break;
                case floppyCmdWriteSector:
                    //fprintf(stdout,"Write sector command\n");
                    // write a sector sets flags ready to recieve the data
                    // the actual sector write does not actually happen until
                    //  all the data has been recieved.
                    // check if drive active
                    if (floppyDrives[floppyActiveDrive].fileNamePointer == NULL ){
                        // no disk mounted in drive TODO what error
                        floppyInteruptRequest = 1; // set interrupt
                        floppyRecordNotFound = 1;
                        floppyBusy = 0;   // no longer busy
                        // fprintf(stdout,"write sector no disk mounted\n");
                        }
                    else if (floppyDrives[floppyActiveDrive].writeProtect != 0 ){
                        // write protected disk mounted in drive TODO what error
                        floppyInteruptRequest = 1; // set interrupt
                        floppyWriteFailWriteProtected = 1;
                        floppyBusy = 0;   // no longer busy
                        //fprintf(stdout,"write sector write protect\n");
                    }
                    // check if tracks match
                    else if ( floppyTrackRegister != floppyDrives[floppyActiveDrive].track){
                        // say not able to write track
                        floppyInteruptRequest = 1; // set interrupt
                        floppyRecordNotFound = 1;
                        floppyBusy = 0;   // no longer busy
                        fprintf(stdout,"write sector mismatch track number register %d drive %d\n",floppyTrackRegister,floppyDrives[floppyActiveDrive].track);
                    }
                    else{

                        int writeSize= floppyDrives[floppyActiveDrive].sizeOfSector;
                        // read all into buffer TODO what if sector size is too big ??
                        if (writeSize > 1024) {
                            writeSize=1024;
                        }
                        floppyBufferUsed = writeSize; // where to stop
                        // load the first element
                        floppyBufferPosition=0; // where to start storing
                        floppyDataRequest=1; //say data avaiable
                        floppyInteruptRequest=0;   // clear if all worked okay
                        //fprintf(stdout,"write sector set\n");
                    }
                    break;
                case floppyCmdWriteSectorMulti:
                    // not doing this at present
                    floppyInteruptRequest = 1; // set interrupt
                    floppyRecordNotFound = 1; // say not found as error
                    break;
                // type 3 commands
                case floppyCmdReadAddress:
                    // read address - create a dummy id record
                    readAddress(value);
                    floppyBusy = 0;   // no longer busy
                    break;
                case floppyCmdReadTrack:
                    // just reset busy
                    floppyBusy = 0;   // no longer busy
                    break;
                case floppyCmdWriteTrack:
                    // dummy code to write track 
                    //fprintf(stdout,"Write sector command\n");
                    // write a sector sets flags ready to recieve the data
                    // the actual sector write does not actually happen until
                    //  all the data has been recieved.
                    // check if drive active
                    if (floppyDrives[floppyActiveDrive].fileNamePointer == NULL ){
                        // no disk mounted in drive TODO what error
                        floppyInteruptRequest = 1; // set interrupt
                        floppyRecordNotFound = 1;
                        floppyBusy = 0;   // no longer busy
                        // fprintf(stdout,"write sector no disk mounted\n");
                        }
                    else if (floppyDrives[floppyActiveDrive].writeProtect != 0 ){
                        // no disk mounted in drive TODO what error
                        floppyInteruptRequest = 1; // set interrupt
                        floppyWriteFailWriteProtected = 1;
                        floppyBusy = 0;   // no longer busy
                        //fprintf(stdout,"write sector write protect\n");
                        }
                    // check if tracks match - not needed for wrtie track
                    //  else if ( floppyTrackRegister != floppyDrives[floppyActiveDrive].track){
                    //    // say not able to write track
                    //    floppyInteruptRequest = 1; // set interrupt
                    //    floppyRecordNotFound = 1;
                    //    floppyBusy = 0;   // no longer busy
                    //    fprintf(stdout,"write track mismatch track number register %d drive %d\n",floppyTrackRegister,floppyDrives[floppyActiveDrive].track);
                    //    }
                    else{
                        // dummy setup
                        int writeSize= floppyDrives[floppyActiveDrive].sizeOfSector;
                        // read all into buffer TODO what if sector size is too big ??
                        if (writeSize > FLOPPYMAXSECTORSIZE) {
                            writeSize=FLOPPYMAXSECTORSIZE;
                        }
                        writeSize=FLOPPYMAXSECTORSIZE; // fix to get most of data
                        floppyBufferUsed = writeSize; // where to stop
                        // load the first element
                        floppyBufferPosition=0; // where to start storing
                        floppyDataRequest=1; //say data avaiable
                        floppyInteruptRequest=0;   // clear if all worked okay
                        //fprintf(stdout,"write sector set\n");
                    }
                    break;
                    // originallly just reset busy
                    // floppyBusy = 0;   // no longer busy
                    // break;

                default:
                    // should not happen but . . . .
                    fprintf(stdout, "floppy controller received unknown command  %2.2X , ignoring it. \n", value);

            }  // end of switch
        } // end of  else

        //displayDetails(" ");
    } // end of else if busy

    else {
        fprintf(stdout, "floppy controller received command  %2.2X  while busy, ignoring it. \n", value);

    }
    // display current floppy status 
    displayfloppyposition();
    // store last commands
    floppyPreviousCommand = value;
    return 0;
}


// output to the set track port E1
static int floppySetTrack(unsigned int value){
// call to set track register
    floppyTrackRegister=value &0xFF; // ensure it is only 1 byte
    if (vfcfloppydebug){
        fprintf(stdout,"Floppy set track  %2.2X \n", value);
    }
    return 0;
}

// output to the set sector port E2
static int floppySetSector(unsigned int value){
// call to set sector register
    floppySectorRegister=value &0xFF; // ensure it is only 1 byte
    if (vfcfloppydebug){
        fprintf(stdout,"Floppy set sector  %2.2X  \n", value);
    }
    return 0;
}

// output to the setdata port E3 
static int floppySetData(unsigned int value){

    // call to write to the data register
    floppyDataRegister=value &0xFF; // ensure it is only 1 byte

    //fprintf(stdout,"set data received  %2.2X  \n",floppyDataRegister);

    // check we are processing a write command
    if ((floppyPreviousCommand & 0xF0) == floppyCmdWriteSector){
        // and we are requesting data
        if (floppyDataRequest == 1){
            // store data into buffer
            // extra check to stop buffer overflow
            floppyDataRequest=0;
            if ( floppyBufferPosition < floppyBufferUsed ){
                floppyBuffer[floppyBufferPosition++] = floppyDataRegister;
            }
            // check if we have reach end of buffer
            // floppyBufferUsed is pointing at the next free location
            // so we need the floppyBufferPosition to be 1 more
            if ( floppyBufferPosition < floppyBufferUsed ){
                // data still to receive
                // floppyDataRequest=1;
                floppyDelayForByteRequest = floppyDelayByteRequestBy; // set to delay the setting of the request data flag
            }
            else {
                // say no more data after this
                floppyDataRequest=0;
                // need to actually write the sector
                if (vfcfloppydisplaysectors){
                    fprintf(stdout,"Write sector\n");
                    showFloppySector();
                }
                // Sector specified by floppyTrackRegister, floppySide, floppySectorRegister
                if ((floppyActiveDrive>-1) && (floppyActiveDrive<4)){
                    if ( floppyTrackRegister != floppyDrives[floppyActiveDrive].track){
                        // say not able to write track
                        floppyRecordNotFound = 1;
                        fprintf(stdout,"Floppy track register %d not matching drive %d actual track %d /n",
                                        floppyTrackRegister, floppyActiveDrive, floppyDrives[floppyActiveDrive].track);
                    }
                    else {
                        // all good
                        writeASector();
                    }
                }
                else { // invalid floppy number
                    floppyRecordNotFound = 1;
                    if (verbose){
                        fprintf(stdout,"write sector:- Invalid drive no %d\n",floppyActiveDrive);
                    }
                }
                // signify end
                floppyInteruptRequest=1;
                floppyBusy=0;
            }
        }
        else {
            if (vfcfloppydebug){
                printf("Setdata received unexpected %2.2X after write sector\n",value);
            }
        }
    }
    else if ((floppyPreviousCommand & 0xF0) ==  floppyCmdWriteTrack){
        // some code to handle the write track command
        // and we are requesting data
        if (floppyDataRequest == 1){

            // decided that we will just take first byte
            // then write a track
            // and set interupt and unset busy 
            if (0){
                // say no more data after this
                floppyDataRequest=0;
                // formattrack();
                // signify end
                floppyInteruptRequest=1;
                floppyBusy=0;
            }
            else{

                // store data into buffer
                // extra check to stop buffer overflow
                floppyDataRequest=0;
                if ( floppyBufferPosition < floppyBufferUsed ){
                    floppyBuffer[floppyBufferPosition++] = floppyDataRegister;
                }
                // check if we have reach end of buffer
                // floppyBufferUsed is pointing at the next free location
                // so we need the floppyBufferPosition to be 1 more
                if ( floppyBufferPosition < floppyBufferUsed ){
                    // data still to receive
                    // floppyDataRequest=1;
                    floppyDelayForByteRequest = floppyDelayByteRequestBy; // set to delay the setting of the request data flag
                }
                else {
                    // say no more data after this
                    floppyDataRequest=0;
                    // need to actually write the sector
                    if (vfcfloppydisplaysectors){
                        fprintf(stdout,"Write track buffer\n");
                        showFloppySector();
                     }
                    formattrack();
                    // signify end
                    floppyInteruptRequest=1;
                    floppyBusy=0;
                }
            
            }
        }
        else {
            if (vfcfloppydebug){
                printf("Setdata received unexpected %2.2X after write track\n",value);
            }
        }
        // ignore if data not requested
    }
    else{
        // store for seek command
        if (vfcfloppydebug){
            fprintf(stdout,"set data received %2.2X  \n",floppyDataRegister);
        }
    }


    return 0;
}

// format the current track on the current drive
static void formattrack(void){

    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4)) {
        int writeSize= floppyDrives[floppyActiveDrive].sizeOfSector;
        if (writeSize > FLOPPYMAXSECTORSIZE) {
            writeSize=FLOPPYMAXSECTORSIZE;
        }
        memset(&floppyBuffer, 0xE5,writeSize );  /* Fill with the E5  */
        
        //floppyDrives[floppyActiveDrive].track;


    }
}

// Wrtie to drive port E4 and E5
static int floppySetDrive(unsigned int value){
// call to set active drive
//  map80 vfc works by setting bits
    switch (value&0x0F){
        case 0x01:
            floppyActiveDrive = 0;
            break;
        case 0x02:
            floppyActiveDrive = 1;
            break;
        case 0x04:
            floppyActiveDrive = 2;
            break;
        case 0x08:
            floppyActiveDrive = 3;
            break;
        default:
            fprintf(stdout,"Floppy set drive invalid value  %2.2X  \n",value);
    }
    // show on the status screen
    displayfloppystatus();
    // TODO look at FM and MFM values ?
    floppyDelayReady = 2; // set ready after 1 read of E4

    if (vfcfloppydebug){
        fprintf(stdout,"Floppy drive set to  %2.2X  \n",floppyActiveDrive);
    }

    // displayDetails("SetDrive");

    return 0;
}

// read status port E0
static int floppyReadStatus(){
// call to read the status register
// Status register is generated dynamically based on indicators set.

    int retval = 0 ;

    static int IndexpulseCountdown=20;


    // update register value with the standard stuff

    if ( (floppyPreviousCommand &0x80) ){

        if (0) {
            printf("previous command TypeII, III, IV - %2X\n",floppyPreviousCommand);
            fprintf(stdout,"status II write %d not found %d CRC %d lost %d \n",
                floppyWriteFailWriteProtected,
                floppyRecordNotFound,
                floppyCRCError,
                floppyDataLost);
        }
        // Type II or II or IV command
        if (floppyWriteFailWriteProtected){
            retval |= FLOPPYSTATUSWRITEPROTECT;
        }
        // TODO deleted data mark
        // record not found
        if (floppyRecordNotFound){
            retval |= FLOPPYSTATUSRECORDNOTFOUND;
        }
        // CRC error
        if (floppyCRCError){
            retval |= FLOPPYSTATUSCRCERROR;
        }
        // data lost
        if (floppyDataLost){
            retval |= FLOPPYSTATUSLOSTDATA;
        }
    }
    else {
        // Type I command
        //printf("previous command TypeI - %2X\n",floppyPreviousCommand);
        if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ) {
            // set if track 0
            // set Head Loaded bit 5
            if (floppyHeadLoaded){
                retval |= FLOPPYSTATUSHEADLOADED;
            }
            else{
                retval &= ~FLOPPYSTATUSHEADLOADED;
            }

            if (floppyDrives[floppyActiveDrive].track==0){
                retval |= FLOPPYSTATUSTRACKZERO;
                // printf("at track 0 \n");
            }

            // is it write protected
            if (floppyDrives[floppyActiveDrive].writeProtect){
                retval |= FLOPPYSTATUSWRITEPROTECT;
            }

            // simple code to pulse the index bit every so often
            IndexpulseCountdown--;
            if (IndexpulseCountdown<5){
                //printf("status %2X\n",retval);
                retval |= FLOPPYSTATUSINDEXFOUND;
                //printf("status %2X\n",retval);
                if (IndexpulseCountdown<0){
                    IndexpulseCountdown=20;
                }

            }
        }

    }

    // is floppy drive ready to run - bit 7
    if (getFloppyReadyFlag()==1){
        // set bit to say not ready
        retval |= FLOPPYSTATUSNOTREADY;
    }
    else {
        retval &= ~FLOPPYSTATUSNOTREADY;
    }
    
    if (SetBusyForTime>0){    // set busy flag for x-1 calls to status - then set floppyInteruptRequest
        SetBusyForTime--;
        if (SetBusyForTime==0){     // if busy delay has got to 0 set floppyInteruptRequest
            floppyInteruptRequest=1;
        }
    }

    // is floppy busy - bit 0
    // set if floppyBusy specifically set or being delayed
    if (floppyBusy || (SetBusyForTime>0)){
        retval |= FLOPPYSTATUSBUSY;
    }
    else {
        retval &= ~FLOPPYSTATUSBUSY;
    }

    if (vfcfloppydebug){
        fprintf(stdout,"Read Status returned  %2.2X  \n",retval);
        //displayDetails("    ");
    }

    return retval;
}

// read track port E1
static int floppyReadTrack(){
// call to read Track register
    if (vfcfloppydebug){
        if (vfcfloppydisplaysectors){
            displayDetails("Read Track");
        }
    // fprintf(stdout,"Read Track Register returned   %2.2X  \n",(floppyTrackRegister&0xFF));
    }
    return floppyTrackRegister&0xFF; // ensure it is only 1 byte
}

// read sector port E2
static int floppyReadSector(){
// call to read Sector register
    //displayDetails("Read Sector");
    //fprintf(stdout,"Read Sector Register returned   %2.2X  \n",(floppySectorRegister&0xFF));
    return floppySectorRegister &0xFF; // ensure it is only 1 byte
}

// read data port E3
static int floppyReadData(){
    // call to read the data register
    // only handles basic sending of current buffer
    // does not support reading the next sector in multi sector read.

    unsigned int retval=0;

    //displayDetails("Read Data");
    if (floppyDataRequest==1){ // we are sending data
        floppyDataRequest=0; // set off for now - played with introducing a delay
        retval = floppyDataRegister; // return current register value 

        // check if we have reach end of buffer
        // floppyBufferPosition will be the position is for the next byte
        // floppyBufferUsed is pointing at the next free loaction
        // We want to stop when floppyBufferPosition is >= floppyBufferUsed
        if ( floppyBufferPosition < floppyBufferUsed ){
            // set up next byte to return
            floppyDataRegister = floppyBuffer[floppyBufferPosition++];
            // data still to send
            floppyDelayForByteRequest = floppyDelayByteRequestBy; // set to delay the setting of the request data flag
            // floppyDataRequest=1;
        }
        else {
            // say no more data after this
            floppyDataRequest=0;
            // signify end
            floppyInteruptRequest=1;
        }
    }
    if (vfcfloppydebug){
        fprintf(stdout,"Read data datarequest %d returned  %2.2X  \n",floppyDataRequest,(retval &0xFF) );
    }

    return retval &0xFF; // ensure it is only 1 byte

}


// read data from the drive port E4 & E5
static int floppyReadDrivePort(){
// call to read Drive port ( DRQ, INTRQ and READY ) using 0xE4 or 0xE5
// need to combine to return

// TODO - fix so if called a lot without other commands happening - bit ??
    static int Numbercalls=0;
    static int lastreturn=0xFF;
    // bit 0 is floppyInteruptRequest, bit 1 is floppyNotReady and bit 7 is floppyDataRequest
    // displayDetails("Read Port");
    
    

    int invertedfloppyNotReady = 0;
    //if (floppyNotReady==0){
    if (getFloppyReadyFlag()==0){
        invertedfloppyNotReady=1;
    }

    // delay saying byte ready or needed by a few checks
    // once floppyDelayForByteRequest is less than 1 it has no impact
    // will be set to 1 or more by the read or write buffer routines
    // Also for Polydos needed to set inverted not ready flag
    // so while data not available it get back 0 
    if (floppyDelayForByteRequest > 0 ){
        floppyDelayForByteRequest -=1;
        if (floppyDelayForByteRequest < 1 ){
            floppyDataRequest=1;
            invertedfloppyNotReady=1; // also say ready
        }
        else {
            floppyDataRequest=0;
            invertedfloppyNotReady=0; // also say not ready overriding normalvalue
        }
    }

    int returnval = floppyInteruptRequest + (invertedfloppyNotReady << 1) + (floppyDataRequest << 7 );

        
    if (vfcfloppydebug){
        if ( lastreturn != returnval){
            if (Numbercalls>1){
                fprintf(stdout,"DrivePort returned value %2.2X  %d times\n", lastreturn, Numbercalls );
            }
            fprintf(stdout,"DrivePort returned value %2.2X delay %d DataRequest %2.2X NotReady %2.2X InteruptReq  %2.2X\n",
                    returnval, floppyDelayForByteRequest, floppyDataRequest,invertedfloppyNotReady,floppyInteruptRequest);
        }
    }
    if (returnval == lastreturn){
        if( (Numbercalls == 1000) ){
            printf("lots of calls to read port E4 - setting drive not ready\n");
            returnval |= 0x02;
        }
        else{
            Numbercalls++;
        }
        
    }
    else{
        lastreturn = returnval;
        Numbercalls=1;
    }
    return returnval;

}


// internal routines

// returns 0 if floppy is ready
static int getFloppyReadyFlag(void){
     // delay saying byte ready or needed by a few checks
    // once floppyDelayReady is less than 1 it has no impact
    // will be set to 1 or more by the read or write buffer routines
    if (floppyDelayReady > 0 ){
        floppyDelayReady -=1;
        if (floppyDelayReady < 1 ){
            return 0;
        }
        else {
            return 1;
        }
    }
    else {
        return 0;
    }

}

// clear all indicators used to set status
static void clearStatusIndcators(){

    floppyInteruptRequest = 0; // set to 1 when intrq should be set
    floppyDataRequest = 0;   // set to 1 when data register available to read or empty to write
    floppyHeadLoaded = 0; // set to 1 when the heads are "locked and loaded"
    floppyCRCError=0;     // set to 1 if there is a crc error
    floppySeekError = 0;  // set to 1 when seek error occured
    floppyRecordNotFound = 0; // set to 1 if record not found

}

// do a step in or out based on floppyStepDirection
// value is the command used and used to see if we update the track register
static void dostep(int value){

    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ) {
        // cannot step out any futher as at track 0
        floppyDrives[floppyActiveDrive].track += floppyStepDirection;
        if (floppyDrives[floppyActiveDrive].track < 0){
           floppyDrives[floppyActiveDrive].track = 0;
        }
        if (floppyDrives[floppyActiveDrive].track > floppyMaxTrackNumber){
           floppyDrives[floppyActiveDrive].track = floppyMaxTrackNumber;
        }
        // update track position
        if  ( value & 0x10 ){
           // update track register
           floppyTrackRegister=floppyDrives[floppyActiveDrive].track;
        }
    }
}


// read sector address
// create a dummy id record in the buffer
// and set up the data read process
static void readAddress(unsigned int command){

    // fprintf(stdout, "Read Address \n");
    floppyInteruptRequest=1;   // set will show if error

    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4) ) {

        // we need to send different sector numbers in case it is scanning all sectors
        // between index markers
        if (readaddresssector==-1){
            // this is first request 
        }
        readaddresssector+=1;
        if (readaddresssector>floppyDrives[floppyActiveDrive].numberOfSectors){
            readaddresssector=0;
            // set index marker
        }

        
        floppyBufferUsed=0;
        floppyBuffer[floppyBufferUsed++]= floppyDrives[floppyActiveDrive].track; // track
        floppyBuffer[floppyBufferUsed++]= floppySide;  // side
        floppyBuffer[floppyBufferUsed++]= floppySectorRegister;


        // set encoded value for length
        floppyBuffer[floppyBufferUsed++]= getsectorsizeLSBvalue (0, floppyDrives[floppyActiveDrive].sizeOfSector);
        // standard value for now !!! TODO
        //floppyBuffer[floppyBufferUsed++]=0x01;
        floppyBuffer[floppyBufferUsed++]= 0x55; // CRC 1 - dummy
        floppyBuffer[floppyBufferUsed++]= 0xAA; // CRC 2
        // the manual says to do this ?
        floppySectorRegister = floppyDrives[floppyActiveDrive].track;
        // setup first byte to return
        // The rest of the process is handled by the readdata register routine
        floppyBufferPosition=0;
        floppyDataRegister = floppyBuffer[floppyBufferPosition++];
        //floppyDataRequest=1;
        // floppyInteruptRequest=0;   // reset to show worked
        floppyDelayForByteRequest = floppyDelayByteRequestBy; // set to delay the setting of the request data flag
        floppyInteruptRequest=0;   // clear if all worked okay

        if (vfcfloppydebug){
            printf("Read address buffer position %d Buffer used %d\n",floppyBufferPosition,floppyBufferUsed);
        }
    }
    else{
        printf("Read address : invalid drive number %d\n",floppyActiveDrive);
    }
}

// Read a sector from floppyActiveDrive at floppyTrackRegister, floppySectorRegister, floppySide.
// Loads the correct sector into the buffer area and sets up the data read process
static void readASector(unsigned int command){
    // TODO should really check that the tracks match but . . .
    // reset status register
    FILE * filepointer=NULL;
    floppyInteruptRequest=1;   // set will show if error
    // we need to find the track and sector off set in the disk
    // position the file at the start of the sector
    if (floppyDrives[floppyActiveDrive].fileNamePointer != NULL ){
        if ( floppyTrackRegister != floppyDrives[floppyActiveDrive].track){
            // say not able to read track
            floppyRecordNotFound = 1;
        }
        else {
            int result=0;
            long int position = floppyFindOffset(floppyActiveDrive,
                    floppyTrackRegister,
                    floppySide,
                    floppySectorRegister);

            if (position < 0 ){
                // invalid something
                floppyRecordNotFound=1;
                fprintf(stdout,"Invalid file pos\n");
            }
            else {
                // filep = fopen(filename, "r+b");

                filepointer = fopen(floppyDrives[floppyActiveDrive].fileNamePointer,"rb");
                if ( filepointer == NULL) {
                    fprintf(stdout, "floppy failed to load '%s', ignoring it. \n", floppyDrives[floppyActiveDrive].fileNamePointer);
                    perror(floppyDrives[floppyActiveDrive].fileNamePointer);
                    // set to drive not ready
                    // floppyNotReady=1;
                    floppyDelayReady=2; // say not ready for 1 call
                    
                } else {
                    result=fseek(filepointer, position, SEEK_SET );
                    if (result !=0 ) {
                        // say not able to read sector
                        floppyRecordNotFound=1;
                        fprintf(stdout,"Read seek error setting Record Not Found\n");
                        perror(floppyDrives[floppyActiveDrive].fileNamePointer);
                    }
                    else {
                        int readSize= floppyDrives[floppyActiveDrive].sizeOfSector;
                        // read all into buffer
                        if (readSize > FLOPPYMAXSECTORSIZE) {
                            readSize=FLOPPYMAXSECTORSIZE;
                        }
                        int numberRead = fread(floppyBuffer,1,readSize,filepointer);
                        if ( numberRead == readSize){
                            floppyBufferUsed= readSize; // where to stop
                            // load the first element
                            floppyBufferPosition=0; // where to start reading
                            // set the first byte to return
                            floppyDataRegister = floppyBuffer[floppyBufferPosition++];
                            //floppyDataRequest=1; //say data avaiable
                            floppyDelayForByteRequest = floppyDelayByteRequestBy; // set to delay the setting of the request data flag
                            floppyInteruptRequest=0;   // clear if all worked okay
                            if (vfcfloppydisplaysectors){
                                fprintf(stdout,"Read sector %d, floppyBufferPosition %d, floppyBufferUsed %d\n",numberRead, floppyBufferPosition , floppyBufferUsed);
                                showFloppySector();
                            }
                        }
                        else {
                            // may be something else but . . . .
                            floppyCRCError=1;
                            floppyRecordNotFound=1;
                            fprintf(stdout,"Record CRC error Read [%d] readsize [%d] \n",numberRead,readSize);
                            perror(floppyDrives[floppyActiveDrive].fileNamePointer);
                        }
                    }
                }
            }
        }
    }
    else { // no disk mounted in drive TODO what error
        floppyRecordNotFound = 1;
        if (verbose) {
            // fprintf(stdout,"no disk mounted\n");
        }

    }
    // ensure file is closed
    if ( filepointer != NULL) {
        fclose (filepointer);
    }

}

// Write a sector to floppyActiveDrive at floppyTrackRegister, floppySectorRegister, floppySide.
// Saves the correct sector from the buffer area
static void writeASector(void){

    FILE * filepointer=NULL;
    // reset status register
    floppyInteruptRequest=1;   // set will show if error

    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4)){
        if (floppyDrives[floppyActiveDrive].fileNamePointer != NULL ){
            int result=0;
            // we need to find the track and sector off set in the disk
            // position the file at the start of the sector
            long int position = floppyFindOffset(floppyActiveDrive,
                    floppyTrackRegister,
                    floppySide,
                    floppySectorRegister);

            if (position < 0 ){
                // invalid something
                floppyRecordNotFound=1;
                fprintf(stdout,"Write Invalid file pos\n");
            }
            else {

                filepointer = fopen(floppyDrives[floppyActiveDrive].fileNamePointer,"r+b");
                if ( filepointer == NULL) {
                    fprintf(stdout, "write floppy failed to load '%s', ignoring it. \n", floppyDrives[floppyActiveDrive].fileNamePointer);
                    perror(floppyDrives[floppyActiveDrive].fileNamePointer);
                    // set to drive not ready
                    // floppyNotReady=1;
                    floppyDelayReady=2; // say not ready for 1 call
                } else {
                    result=fseek(filepointer, position, SEEK_SET );
                    if (result !=0 ) {
                        // say not able to read sector
                        floppyRecordNotFound=1;
                        fprintf(stdout,"Seek error settting record not Found\n");
                        perror(floppyDrives[floppyActiveDrive].fileNamePointer);
                    }
                    else {
                        int writeSize= floppyDrives[floppyActiveDrive].sizeOfSector;
                        // write all into buffer
                        if (writeSize > FLOPPYMAXSECTORSIZE) {
                            writeSize=FLOPPYMAXSECTORSIZE;
                        }
                        int numberWritten = fwrite(floppyBuffer,1,writeSize,filepointer);
                        if ( numberWritten != writeSize){
                            // may be something else but . . . .
                            floppyCRCError=1;
                            floppyRecordNotFound=1;
                            fprintf(stdout,"write error - Record CRC error Witten [%d] writesize [%d] \n",numberWritten,writeSize);
                            perror(floppyDrives[floppyActiveDrive].fileNamePointer);
                        }
                    }
                }
            }
        }
        else { // no disk mounted in drive TODO what error
            floppyRecordNotFound = 1;
            if (verbose){
                fprintf(stdout,"no disk mounted in drive %d\n",floppyActiveDrive);
            }

        }
    }
    else { // invalid floppy number
        floppyRecordNotFound = 1;
        if (verbose){
            fprintf(stdout,"Invalid drive no %d\n",floppyActiveDrive);
        }
    }
    // ensure file is closed
    if ( filepointer != NULL) {
        fclose (filepointer);
    }

}



static void showFloppySector(){

    printf( "floppy drive %d Position:- Track %02X Head %02X Sector %02X\n",
             floppyActiveDrive,
	         floppyTrackRegister,
	         floppySide,
	         floppySectorRegister );
    if ((floppyActiveDrive>-1) && (floppyActiveDrive<4)) {
        printf( "floppy drive %d Actual:- Track %02X\n",
                 floppyActiveDrive,
                 floppyDrives[floppyActiveDrive].track);
    }
    displayBuffer( floppyBuffer, floppyBufferUsed );
}

static void displayBuffer(unsigned char buffer[], int length ){
    int addr = 0; // could use this to show offset into disk image..
    for (int i = 0; i<length/32; i++) {
        fprintf(stdout,"%08x: ", addr+i*32);
        for (int j = 0; j<32; j++) {
            if (j%8 == 0) {
                fprintf(stdout," ");
            }
            fprintf(stdout,"%02x ",buffer[i*32+j]);
        }
        fprintf(stdout," ");
        for (int j = 0; j<32; j++) {
            if (j%8 == 0) {
                fprintf(stdout," ");
            }
            if ((buffer[i*32+j] < 0x7f) && (buffer[i*32+j] > 0x1f)) {
                fprintf(stdout,"%c", buffer[i*32+j]);
            }
            else {
                fprintf(stdout,".");
            }
        }
        fprintf(stdout,"\n");
    }
}

// we need to calculate offset into image file
// using data store floppyDrives[]
static long int floppyFindOffset(int DriveNumber,int Track, int discSide, int Sector){

    long int position = -1;

    long int sizeOfTracks=0;
    long int startTrack=0;
    long int startSector = 0;
    int realdiscside= discSide;
    int realtrackno = Track;

    if ((DriveNumber < 0) && (DriveNumber >= NUMBEROFDRIVES)){
        // invalid drive specified
        printf("invalid drive no %d\n",DriveNumber);
    }
    else {
        
        // calculate the size of each track per side
        sizeOfTracks=floppyDrives[DriveNumber].sizeOfSector * floppyDrives[DriveNumber].numberOfSectors;
        if (floppyDrives[DriveNumber].sidesswapped == 1){
            // sides are swapped 0 becomes 1 and 1 becomes 0
            realdiscside = ( discSide + 1 ) & 0x01;
            printf("Real disc side %d",realdiscside);
        }
        // handle reverse sides
        if (realdiscside==0){
            if (floppyDrives[DriveNumber].reverseside0==1){
                // switch tracks
                realtrackno= floppyDrives[DriveNumber].numberOfTracks - Track;
                printf("RS0: Real track from %d to  %d",Track,realdiscside);
            }
        }
        if (realdiscside==1){
            if (floppyDrives[DriveNumber].reverseside1==1){
                // switch tracks
                realtrackno= floppyDrives[DriveNumber].numberOfTracks - Track;
                printf("RS1: Real track from %d to  %d",Track,realdiscside);
            }
        }

        // check in interleaved
        if (floppyDrives[DriveNumber].interleaved == 1){

            // calculate where the current track side 0 starts in the file
            // track interleaved means skip 2 tracks for each "track"
            startTrack=sizeOfTracks * (realtrackno * floppyDrives[DriveNumber].numberOfHeads);
            // calculate if we need to skip side 0 
            startTrack += (sizeOfTracks * discSide);
            // calculate where the sector starts in that track
            startSector = floppyDrives[DriveNumber].sizeOfSector * floppySectorRegister;
            // sort out the file position for that sector
            position = startTrack + startSector;
        }
        else {
            // floppy is sequential side 0 then side 1
            // calculate where the current track side 0 starts in the file
            // track Sequential means just * 1 for each "track"
            startTrack=sizeOfTracks * realtrackno;
            // calculate if we need to skip side 0
            startTrack += (sizeOfTracks * floppyDrives[DriveNumber].numberOfTracks * discSide);
            // calculate where the sector starts in that track
            startSector = floppyDrives[DriveNumber].sizeOfSector * floppySectorRegister;
            // sort out the file position for that sector
            position = startTrack + startSector;
        }
        // check if gone for head 1 when only 1 head - after head swop ??????
        if (realdiscside > floppyDrives[DriveNumber].numberOfHeads ){
            printf("invalid head number %d drive no %d\n",realdiscside,DriveNumber);
            position=-1;
        }
        if (vfcfloppydebug){
            //fprintf(stdout,"Record position %ld + %ld = %ld\n",startTrack, startSector,(startTrack + startSector) );
            fprintf(stdout,"Record position for Track %d Head %d Sector %d is %ld\n",Track, discSide,Sector, position );
        }
    }
    return position;
}

int getsectorsizeLSBvalue(int sectorLengthFlag, unsigned int sectorsize){

    // the length seems to be related to the bit in the command
    // only on reads and write sectors - incorrect here !!!!!
    // this value would be stored on the sector header data
    // TODO - how to get the sector length flag
    
    int lengthLSBvalue=0;
    if (sectorLengthFlag!=0) {
        // command is 1 for length
        switch (sectorsize){
            case 128:
                lengthLSBvalue=0;
                break;
            case 256:
                lengthLSBvalue=1;
                break;
            case 512:
                lengthLSBvalue=2;
                break;
            case 1024:
                lengthLSBvalue=3;
                break;
            default:
                lengthLSBvalue=2;
                break;
        }
    }
    else{
        // command is 0 for length
        switch (sectorsize){
            case 256:
                lengthLSBvalue=0;
                break;
            case 512:
                lengthLSBvalue=1;
                break;
            case 1024:
                lengthLSBvalue=2;
                break;
            case 128:
                lengthLSBvalue=3;
                break;
            default:
                lengthLSBvalue=1;
                break;
                
        }
    }
    if (vfcfloppydebug){
        printf("ReadAddress Length flag %2.2X, resulted in sector length field %2.2X \n",sectorLengthFlag ,lengthLSBvalue);
    }
    return lengthLSBvalue;
}



// end of code


