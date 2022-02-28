/*
 * handle the MAP80 256k memory card
 * 
 * This emulates a 256k memory card using port 0xFE
 * 
 * You can increase the memory by  changing the virtualram size
 * see VIRTUALRAMSIZE in options.h
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdbool.h>

#include "options.h"  //defines the options to usemap80RamIntialise
#include "simz80.h"
#include "map80ram.h"
#include "map80nascom.h"
#include "statusdisplay.h"

int rampagedebug=0;

void displayRamTable();


/* handle the page mapping required for the MAP80 256k ram card
   bit 0   set selects upper 32k    ) only if in 32k mode
           reset selects lower 32k  )
   bit 1 )
   bit 2 )
   bit 3 ) selects the 64k page to be used
   bit 4 )
   bit 5 ) not sure about bit 5 ?

   bit 6    set  select upper 32k of page 0 as permanent
            reset  select lower 32k of page 0 as permanent

   bit 7    set  select 32k page mode
            reset select 64k page mode

   The process handles the movement by changing the pointers in rampagetable.
   All memory is in the ram array

   Need to check ramlocktable for same element in rampagetable
   if set to 1 then the rampagetable pointer cannot be updated
   i.e. for monitor romramromtable

*/

// using Memory Management Unit we have a set of pointers to point to ram
// each entry in ramapagetable points to the start of a page in ram
// will be 2k if PAGESIZE is 2
// For Nascom areas the rampagetable entry will be pointing at special memory area
// Then the ramlocktable will be set to 1 stop the memory management system changing to this pointer
// For ROMS the ramromtable will be set to 1 to stop writes to that memory area.
BYTE *rampagetable[RAMPAGETABLESIZE];
// we need a second table of pointers for the ram if the RAMDISABLE from Nascom was not active
// It will point to the address in ram to use if MAP80 256k card was the active memory.
BYTE *ramdefaultpagetable[RAMPAGETABLESIZE];
// this is the total ram space to be used for memory management
BYTE virutalram[VIRTUALRAMSIZE*1024];

// fix DA
// ramromtable is set to 0 if you can write to it
// otherwise a write attempt is just ignored
// i.e. that area is a ROM
// set to 1 for permanent ROM - i.e. monitor
// set to 2 for temporary when page mapping attempted past end of virtual memory
int ramromtable[RAMPAGETABLESIZE];

// ram lock table is set to 0 if you can change it's pointer in rampagetable
// otherwise the rampagetable will not be changed
// acts like the N2 ram disable line.
// this should be linked to a seperate 2k block and not the normal ram area.
// as the virtual ram can be switch from lower to upper 32k blocks and visa vera
// this would mean that the "rom" could appear in an area that should be ram 
int ramlocktable[RAMPAGETABLESIZE];

// next 2 areas only referenced in virutal-nascom code
// defines some space for NASCOM Monitor ROM, video ram and working Ram
BYTE NascomMonVWram[4*1024];

// this area of ram is set to show that ram does not exist
// rampagetable can be pointed at it and it will return dummy value
// but we will set the ramromtable so that it cannot be written to
BYTE dummyram[RAMPAGESIZE*1024];

// the status screen 
BYTE statusdisplayram[STATUS_DISPLAYRAMSIZE];
// space for the VFC rom and screen memory
BYTE vfcrom[2*1024];
// allowed 2k for display ram 
// the display chip can address more ?? TODO - think about it 
BYTE vfcdisplayram[2*1024];


 // intialise the map80 memory card.
void map80RamInitialise(){



    // intialise rampagetable to point at the first 64k of ram
    for (int c=0; c< (RAMPAGETABLESIZE) ; ++c) {
        // first the default table if no ramdisable is active
        ramdefaultpagetable[c]=virutalram+(c<<RAMPAGESHIFTBITS);
        // then the memory pointers
        rampagetable[c] = ramdefaultpagetable[c];
 	    //	 debug to show values generated
        if (rampagedebug){
            fprintf(stdout, "rampagetableindex [%02x], ram [%p], ram address [%p] \n",
                           c, virutalram, rampagetable[c] );
        }

    }

    // sets the memory used to tell us we are not in real memory
    // used if we try and set map80 256k card to values where no card exists
    memset(dummyram, 0x76, RAMPAGESIZE*1024 );  /* Fill dummy ram with the halt instruction */
    // set ram areas to HALT
    //	 debug to show values generated
    if (rampagedebug){
        printf("size of NascomMonVWram ram %ld \n", sizeof NascomMonVWram);
    }
    memset(&NascomMonVWram, 0x76,sizeof NascomMonVWram );  /* Fill with the halt instruction */
    // when nasbug T4 or nas-sys monitors start up, they restore the breakpoint byte at the
    // breakpoint address. The effect is to write 76H to address 7676H. Usually this is not
    // a problem. However, for the case where a command-line argument is used to load a .nas
    // file, and that file loads 7676H, the monitor start up will corrupt the data at that
    // address. To avoid this nastiness, set brkadr to 0 (which is read-only and so will
    // not be affected by the write of 76H.
    NascomMonVWram[0x0c15]=0; // nasbug T4
    NascomMonVWram[0x0c16]=0;
    NascomMonVWram[0x0c23]=0; // nas-sys
    NascomMonVWram[0x0c24]=0;

    if (rampagedebug){
        printf("size of virtual ram %ld \n", sizeof virutalram);
    }
    memset(&virutalram, 0x76,sizeof virutalram );  /* Fill with the halt instruction */

    // The vfc extra areas
    if (rampagedebug){
        printf("size of vfc rom %ld \n", sizeof vfcrom);
    }
    memset(&vfcrom, 0x76,sizeof vfcrom );  /* Fill with the halt instruction */
    if (rampagedebug){
        printf("size of vfc display %ld \n", sizeof vfcdisplayram);
    }
    memset(&vfcdisplayram, 0x76,sizeof vfcdisplayram );  /* Fill with the halt instruction */

    // show where the pointers are pointing at
    if (rampagedebug){
        displayRamTable();
    }

    return ;
}


// handle the changes to the map80 ram card memory mapping
void map80Ram(unsigned char value){



    // find the first entry in ram array
    // take bits 1 to 5, shift right 1 to create the 64k page number
    int page64knumber = ((value & 0x3E) >> 1 );
    // find the first 64k page in the ram array
    // multiple by (64 * 1024)  = 65536 which is shift 16 left
    BYTE *ramaddress = virutalram + ( page64knumber << 16 );

    // rampagetable entry
    // starts at 0 - there is only 32 (64 / 2) entries
    int rampagetableindex = 0;

    // number of pages to update if in 64k mode ( 64/2 ) = 32
    int numberToUpdate = 32;

    if (rampagedebug){
        // debug to show values generated
        //fprintf(stdout, "Port Data [%2.2x], 64k page [%2.2x], rampagetableindex [%2.2x], ram address [%p], ram offset [%4.4X] \n",
        //                       value, page64knumber, rampagetableindex, virutalram, (int32_t) (ramaddress-virutalram));
        fprintf(stdout, "Port Data [%02X], 64k page [%02X], rampagetableindex [%02X], numbertoupdate [%02X], virtual ram address [%p], ramoffset [%4.4X] \n",
                           value, page64knumber, rampagetableindex, numberToUpdate,  virutalram, (int32_t)(ramaddress-virutalram) );
    }

    // check if 32 or 64k mode
    if ( (value & 0x80) == 0x80 ) {
        // 32k page mode as bit 7 set
        numberToUpdate = 16; // we move fewer entries in 32k mode
        // check if updating the upper or lower 32k entries in rampagetable
        // start address is correct if updating the lower 32k
        if  ( (value & 0x40) != 0x40 ){
            // lower 32k is fixed - update the entries in the 32k of rampagetable
            // need to move the rampagetableindex up 32 / 2 = 16 slots
            rampagetableindex += 16;
        }
	// check if moving in the lower or upper 32k of that page
        // if moving in the lower than already set
        if ( (value & 0x01) == 0x01 ) {
            // moving in the upper 32k entries
            // add 32k to the ram address (32 * 1024 = 32768 )
            ramaddress +=  32768;
        }
    }

    if (rampagedebug){
        // debug to show values generated
        fprintf(stdout, "Port Data [%02X], 64k page [%02X], rampagetableindex [%02X], numbertoupdate [%02X], virtual ram address [%p], ramoffset [%4.4X] \n",
                           value, page64knumber, rampagetableindex, numberToUpdate,  virutalram, (int32_t)(ramaddress-virutalram) );
    }
    // if the requested page is greater than the virtual ram
    //  256k card will generate page64knumber from 0 to 3
    if ( page64knumber >= VIRTUALRAMSIZE / 64 ) {
	// page requested is past end of memory
        // set it to dummy memory
            ramaddress = dummyram;
	}
    // now actually update the pagetableentries
    int indextostopbefore = rampagetableindex + numberToUpdate;

    for (int tableindex = rampagetableindex ; tableindex < indextostopbefore ; tableindex ++ ) {

        // first the default table if no ramdisable is active
        ramdefaultpagetable[tableindex]=ramaddress;

 	    //	 debug to show values generated
        if (rampagedebug){
            fprintf(stdout, "Ramdefault Index [%02X], set to  [%p] \n",
                   tableindex, ramaddress );
        }

        if ( ramlocktable[tableindex] == 0 ) {
            // the ram is not locked so update the pointer
            rampagetable [tableindex] = ramaddress;
     	    //	 debug to show values generated
            if (rampagedebug){
                fprintf(stdout, "Rampage Index [%02X], set to  [%p] \n",
                       tableindex, ramaddress );
            }
            // set lock mode
            if (ramaddress == dummyram){
                // set to prevent r/w
                // if it is rom it would also have ramlocktable set
                // use 2 for debug purposes
                ramromtable[tableindex] = 2;
            } else {
                // allow the area r/w in case it was set last time
                ramromtable[tableindex] = 0;
            }
        }
        // now move on ram address - if needed
        if (ramaddress != dummyram){
            // increment the ram address by 2k for the next table entry
            ramaddress += 2048;
        }

    }
    //	 debug to show values generated
    if (rampagedebug){
        displayRamTable();
        fprintf(stdout, "\n");
    }

    return;
}

void displayRamTable(){


    for (int c=0; c< (RAMPAGETABLESIZE) ; ++c) {
        unsigned int offset1 = rampagetable[c]-virutalram;
        unsigned int offset2 = ramdefaultpagetable[c]-virutalram;
        fprintf(stdout, "rampagetableindex [%02x], ram [%p], ram address [%4.4X] default [%4.4X] \n",
                           c, virutalram, offset1, offset2 );
    }



}


