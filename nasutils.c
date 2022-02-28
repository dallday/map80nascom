/* utilities to handle .nas type files
  ??  calls ihex to handle the ihex format
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdbool.h>
#include "nasutils.h"
#include "simz80.h"
#include "map80nascom.h"
#include "utilities.h"

// Expect a line of text from a NAS file. It should start with 1, 4-digit hex address and either
// 9, 2-digit hex data bytes or fewer. In the case of 9 the 9th is a checksum byte. If the
// checksum is present, check it and report an error if it's bad.
// return 0 if address and at least 1 data byte and no checksum error OR if valid end
// otherwise return 1.
// bytes[] holds 8 elements; count is the number of valid bytes
int parseNASline(int line, const char *buf, int *address, int *count, unsigned int *bytes)
{
    unsigned int checksum;
    *count = sscanf(buf, "%4x %2x %2x %2x %2x %2x %2x %2x %2x %2x", address,
                    &bytes[0], &bytes[1], &bytes[2], &bytes[3],
                    &bytes[4], &bytes[5], &bytes[6], &bytes[7],
                    &checksum);

    if ((buf[0] == '.') || (buf[0] == '\r')) {
        // terminator or blank line - OK
        *count = 0;
        return 0;
    }

    if (*count < 2) {
        // address only or not even that - bad
        *count = 0;
        return 1;
    }

    if (*count == 10) {
        // address, 8 data and checksum
        unsigned int calcsum;
        *count = 8;
        calcsum = (*address >> 8) + (*address & 0xff);
        for (int i=0; i<8; i++) {
            calcsum += bytes[i];
        }
        if ((calcsum & 0xff) == checksum) {
            return 0;
        }
        else {
            if (verbose) printf("\tError on line %d - calculated checksum 0x%2X does not match 0x%2X\n",line, calcsum & 0xff, checksum);
            return 1;
        }
    }

    // address, 1-8 data
    *count = *count - 1;
    return 0;
}


// load a Nascom NAS format file into memory.
// It can be any filename.
// The extention is normally .nas, but can be .nal
// A file with a .rom extention will be loaded into its own ROM space.
//   That makes it Read only and locked in the memory space - like the EPROMS on the Nascom 2 board.
//   The limits are it must start on a 2k boundary in memory and must not be more than 8k long.
// 
// The file format should be an address then 8 bytes and an optional checksum all as ASCII 0-9 a-f
// Currently it does not like where the last line is not 8 bytes - will ignore it
// returns 0 if all okay anything else is a failure
// will load into ram space but if .nal file it will create a rom space 
// and copy it from ram space to there.
int loadNASformat(const char *filetoload)
{
    int retval=0;
    int firstaddress=-1;
    int lastaddress=-1;
    char fileext[12]="";
    int count1=0;
    int count2=0;
    int namelen=0;

    namelen=strlen(filetoload);
    if (verbose) printf("Loading %s\n", filetoload);

    // find the . in the file name
    for (count1=namelen;count1>0;count1--){
        if (filetoload[count1]=='.') {
            break;
        }
    }
    
    count1++; // allow for the .
    // check if we have any extension 
    if (count1<1){
        if (verbose) printf("\tWarning - No extention on file %s\n", filetoload);
        count1=namelen;
    }
    // check extention name not too long
    if ((namelen-count1)>10){
        if (verbose) printf("\tWarning - extention on file %s too long, max 10\n", filetoload);
        count1=namelen;
    }
    
    // copy it over converting it to lower case 
    for (count2=0;count1<namelen;count1++ , count2++){
        fileext[count2]=mytolower(filetoload[count1]);
    }
    fileext[count2]=0; // mark end

    // split it out to make it easier to read the code :)
    retval=loadNASformatinternal(filetoload,&firstaddress,&lastaddress );
    
    // printf("loaded %s from %4.4X to %4.4X \n",filetoload,firstaddress,lastaddress);
    // allocate ROM type ( .nal file ) their own space when loading.
    // Cannot just lock the area in the first 64k - as we can move that into 32k lower or uppers :)
    // and it would appear in none locked areas.
    if (retval==0){
        if (verbose) printf("\tLoaded %s into memory at address 0x%4.4X\n",filetoload,firstaddress);
        // check if it was a .nal type file
        if ( strcmp( fileext,"rom") == 0 ){
            // allocate some extra memory for it 
            int memoryused = lastaddress-firstaddress + 1;
            if (verbose) printf("\tmemory used 0x%4.4X\n",memoryused);
            // check that fist address is on a 2k boundary to calculate rampages to set
            // also if > 0x1000
            // if not could upset the nascom stuff if it tries to be rom
            if (firstaddress < 0x1000){
                if (verbose) printf("\tcannot activate as ROM as below address 0x1000\n");
            }
            else if ((firstaddress % 2048) > 0 ) {
                if (verbose) printf("\tcannot activate as ROM as not starting on a 2k boundary\n");
            }
            else if (memoryused > (8*1024)  ) {
                if (verbose) printf("\tcannot activate as ROM as larger than 8k\n");
            }
            else {
                // the memory must be in 2k chunks
                int remainder =(memoryused % 2048);
                if (remainder > 0 ) {
                    memoryused=((memoryused/2048)+1)*2048;   // add extra 2k 
                }
                if (verbose) printf("\tmemory used now 0x%4.4X actual 0x%4.4X\n",memoryused,(unsigned int)(memoryused  * sizeof(BYTE)));
                // allocate memory for the rom
                BYTE * newmemory = malloc(memoryused  * sizeof(BYTE)); 
                if (newmemory==NULL){
                    // whoops that failed 
                    printf("\tWarning - Unable to allocate 0x%4.4X ROM space for %s\n",(unsigned int)(memoryused  * sizeof(BYTE)),filetoload);
                }
                else{
                    // memory allocated 
                    // copy the info from the virtual memory to the rom space
                    // used the slow method as the ram may not be contiguous ?
                    int copycount=0;
                    for (copycount=0;copycount<memoryused;copycount++){
                        newmemory[copycount]=RAM(firstaddress+copycount);
                        RAM(firstaddress+copycount)=0x76;   // reset it to HALT 
                    }
                    copycount--;    // backup 1
                    // printf("Last mem copied add %4.4X count %4.4X value %2.2X \n",firstaddress+copycount,copycount,newmemory[copycount]);
                    // now point at it from rampagetable
                    // which entry do we need to use
                    int rampageentry = ((firstaddress)>>RAMPAGESHIFTBITS) & RAMPAGETABLESIZEMASK;
                    // printf("ram page table entry %2.2X\n",rampageentry);

                    for (int memoryusedcopy=memoryused ;memoryusedcopy>0 ;memoryusedcopy-=(RAMPAGESIZE*1024), rampageentry++ )  {
                        if (rampageentry<RAMPAGETABLESIZE){
                            rampagetable[rampageentry]=newmemory;
                            ramromtable[rampageentry]=1;    // say it is rom
                            ramlocktable[rampageentry]=1;   // and active nas ram disable 
                            //printf("rampage table %2.2x set to point to %p\n",rampageentry,newmemory);
                        }
                        else{
                            // should not happen but - - - -
                            printf("\tError - rampageentry %d past top of RAMPAGETABLESIZE %d\n",rampageentry,RAMPAGETABLESIZE);
                            break;
                        }
                        newmemory+=(RAMPAGESIZE*1024);
                    }
                    if (verbose) printf("\tLoaded into ROM at address 0x%4.4X for 0x%2.2X bytes\n",firstaddress,memoryused);
                }
            }
        }
    }
    else {
        if (verbose) printf("\tWarning - problem loading %s\n",filetoload);
    }
    return retval;
}

// process the file and returns first (lowest) and last (highest) address used
int loadNASformatinternal(const char *filetoload, int *firstaddressused, int *lastaddressused )
{
    int address;
    int validbytes;
    unsigned int bytes[8];
    int lineNumber = 0;

    int retval = 0;   // defaults to all okay
    int totalbytes = 0;

    int firstaddress = 100000;
    int lastaddress = -1;

    char line[80]; // one line from the .nas file

    FILE *f = fopen(filetoload, "r");

    if (!f) {
        perror(filetoload);
        return 1;
    }

    // process file line by line
    while (fgets(line, sizeof line, f) != NULL) {
        lineNumber++;
        retval |= parseNASline(lineNumber, line, &address, &validbytes, bytes);

        if (firstaddress > address) {
            firstaddress=address;
        }

        for (int i=0; i<validbytes; i++) {
            RAM(address)=bytes[i];
            if (lastaddress<address){
                lastaddress=address;
            }
            address++;
            totalbytes++;
        }
    }

    fclose(f);
    if (firstaddress>0xFFFF){
        if (verbose) printf("\tWarning: No data loaded\n");
        retval=1;
    }
    if (verbose){
        if (retval){
            printf("\tError(s) during load. Loaded %d bytes into memory (0x%04X - 0x%04X)\n", totalbytes, firstaddress, lastaddress);
        } else {
            printf("\tSuccessfully loaded %d bytes into memory (0x%04X - 0x%04X)\n", totalbytes, firstaddress, lastaddress);
        }
    }

    // the address range used
    *firstaddressused = firstaddress;
    *lastaddressused = lastaddress;
    return retval;
}

// load a nas file into a specific part of memory -- used to load the nassys3 and map80vfc rom file
int loadNASformatspecial(const char *filetoload, unsigned char *memory, int memorySize)
{
    int address;
    int validbytes;
    unsigned int bytes[8];
    int lineNumber = 0;

    int retval = 0;   // defaults to all okay
    int totalbytes = 0;

    if (verbose) printf("Loading %s\n", filetoload);

    char line[80]; // one line from the .nas file

    FILE *f = fopen(filetoload, "r");

    if (!f) {
        perror(filetoload);
        return 1;
    }

    // process file line by line
    while (fgets(line, sizeof line, f) != NULL) {
        lineNumber++;
        retval |= parseNASline(lineNumber, line, &address, &validbytes, bytes);

        for (int i=0; i<validbytes; i++){
            if (address < memorySize){
                memory[address] = bytes[i];
                address++;
                totalbytes++;
            }
            else {
                if (verbose) printf("\t%s Address 0x%1X on line %d passed end of memory size 0x%1X \n",
                                    filetoload,address,lineNumber,memorySize);
                retval=1; // signify error - but process rest of file
                break;
            }
        }
    }

    fclose(f);
    if (verbose){
        if (totalbytes<1){
            printf("\tWarning: No data loaded\n");
        }
        else {
            if (retval){
                printf("\tError(s) during load. Loaded %d bytes into its own memory\n", totalbytes);
            } else {
                printf("\tSuccessfully loaded %d bytes into its own memory\n", totalbytes);
            }
        }
    }
    return retval;
}

// end of nasutils.c
