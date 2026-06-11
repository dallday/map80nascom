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
void resetMemoryPages();

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
// May 2026 DA N4 switched to PAGESIZE is 2 and 1k pages 
// For Nascom areas the rampagetable entry will be pointing at special memory area
// Then the ramlocktable will be set to 1 stop the memory management system changing to this pointer
// For ROMS the ramromtable will be set to 1 to stop writes to that memory area.
BYTE *rampagetable[RAMPAGETABLESIZE];
// we need a second table of pointers for the ram if the RAMDISABLE from Nascom was not active
// It will point to the address in ram to use if MAP80 256k card was the active memory.
// DA N4 - not sure but this should be the current "mapped" address 
// so we can put it back if another area is taken out
BYTE *ramdefaultpagetable[RAMPAGETABLESIZE];
// this is the total ram space to be used for memory management
BYTE virutalram[VIRTUALRAMSIZE*1024];

// fix DA
// ramromtable is set to 0 if you can write to it
// otherwise a write attempt is just ignored
// i.e. that area is a ROM
// set to 1 for permanent ROM - i.e. monitor
// set to 2 for temporary when page mapping attempted past end of virtual memory
// set to what it should be what ever ram/rom page may be overlaying it
// this is then used if the overlating rom/ram is removed
int ramromdefaulttable[RAMPAGETABLESIZE];
// this is what is set if the ram/rom page is not overlaying it
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
// DA N4 - added SBROM to the end of NascomMonVWram so +1000
// changed mind again 
BYTE NascomMonVWram[5*1024];

// DA N4
// area for the SBROM for N4
BYTE SBROM[1*1024];

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

    // intialise rampagetable and ramdefaultpagetable to point at the first 64k of ram
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
        // DA N4 initialise the ramromtable and ramlocktable
        // set so they can be mapped and written to
        ramromtable[c]=0;
        ramromdefaulttable[c]=0;
        ramlocktable[c]=0;
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
    // DA N4 now doing 1k pages so 64 not 32
    int rampagetableindex = 0;

    // number of pages to update if in 64k mode ( 64/2 ) = 32
    // DA N4 now doing 1k pages so 64 not 32
    int numberToUpdate = 64;

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
        numberToUpdate = 32; // DA N4 now 32 not 16 we move fewer entries in 32k mode
        // check if updating the upper or lower 32k entries in rampagetable
        // start address is correct if updating the lower 32k
        if  ( (value & 0x40) != 0x40 ){
            // lower 32k is fixed - update the entries in the 32k of rampagetable
            // need to move the rampagetableindex up 32 / 2 = 16 slots
            // DA N4 now 1k so 64 slots /2 = 32
            rampagetableindex += 32;
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
        if (ramaddress == dummyram){
            // stop writing as a dummy area
            // set to prevent r/w
            // if it is rom it would also have ramlocktable set
            // use 2 for debug purposes
            // DA N4 - changed to or in case other bits are set 
            // this will set bit 2 but leave the rest alone
            // not sure ?
            // DA N4 introduced the ramromdefaulttable[]
            //  as now might have roms being disabled
            ramromdefaulttable[tableindex] |= 0x02;

        } else {
            // allow the area r/w in case it was set last time
            // DA N4 - changed to and in case other bits are set 
            // will reset bit 2 only ( FD == FF-2)
            // allow read write
            ramromdefaulttable[tableindex] &= 0xFD;
        }
        
        if ( ramlocktable[tableindex] == 0 ) {
            // the ram is not locked so update the pointer
            rampagetable [tableindex] = ramaddress;
     	    //	 debug to show values generated
            if (rampagedebug){
                fprintf(stdout, "Rampage Index [%02X], set to  [%p] \n",
                       tableindex, ramaddress );
            }
            // set lock mode from default mode
            ramromtable[tableindex] =ramromdefaulttable[tableindex];
        }

        // now move on ram address - if needed
        if (ramaddress != dummyram){
            // increment the ram address by 2k for the next table entry
            // DA N4 MAY 2026 now using 1k pages 
            // should the increment be a varable or defined
            ramaddress += 1024;
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

    fprintf(stdout,"pampagetable where not default value\n");
    fprintf(stdout," fullramarea address [%p] \n",  virutalram );
    for (int c=0; c< (RAMPAGETABLESIZE) ; ++c) {
        unsigned int offset1 = rampagetable[c]-virutalram;
        unsigned int offset2 = ramdefaultpagetable[c]-virutalram;
        // display if any of the bits are nor the default values 
        if ((offset1 != offset2)||(ramlocktable[c]!=0x0)||(ramromtable[c]!=0x0)){
        /*    
            fprintf(stdout, "rampagetableindex [%02x], Lock [%02x], Rom [%02x], ram [%p], ram address [%4.4X] default offset [%4.4X] \n",
                           c, 
                           ramlocktable[c],ramromtable[c],
                           virutalram, rampagetable[c]-0, offset2 );
          */                 
            fprintf(stdout, "rampagetableindex [%02x], Address [%04x], Lock [%02x], Rom [%02x], ram address [%p] default [%p] \n",
                           c, c<<RAMPAGESHIFTBITS,
                           ramlocktable[c],ramromtable[c],
                           rampagetable[c], ramdefaultpagetable[c] );
        }
    }



}

/*
 * routine to ensure all rampagetable entries are set to default values
 * used at the start of cold or warm restart 
 * this ensures that any ram paging is put back to where it should be
 * problem is when basic.rom etc has been loaded and we need to not clear those areas
 * 
 */

void resetMemoryPages(){

    // set all the default memory pages back to page 0
    
    for (int c=0; c< (RAMPAGETABLESIZE) ; ++c) {
        ramdefaultpagetable[c]=virutalram+(c<<RAMPAGESHIFTBITS);
    }
    
    for (int c=0; c< (RAMPAGETABLESIZE) ; ++c) {
        // check if rom active 
        if (ramlocktable[c]==0){
            // set the memory pointers to default values
            rampagetable[c] = ramdefaultpagetable[c];
        }
    }
    
    
}



// end of code