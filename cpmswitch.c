/*
 * DA N4 changed to handle the mapping in of the nascom file area and SBROM 
 * 
 * handles the NASSYS ROM, Nascom video and nascom working ram
 * handles the N4 SBROM 
 * 
 */

#include "simz80.h"
#include "map80ram.h"
#include "cpmswitch.h"

/*

 * DA N4 nolonger using this but 
 * the N4 port_PROTECT bit 5
int cpmswitchstate=0;          // set to 1 if cpm mode set
                              // vfc boot rom in at 0 to start
                             // Nascom ROM and VWRAM not enabled
                            //

void setcpmswitch(int state){ // set the mode for cpm switch 
    // 0 - NASSYS
    // 1 - cpm - map80vfc link 4 is set to bring in the rom at start
    
    if (state==0){

        // DA N4 using new functions
        EnableNascomMonitor();
        EnableNascomWorkRam();
        EnableNascomVideoRam(unsigned char value);

    }
   // tell the rest if the code we are in cpm mode
    cpmswitchstate=state;
    
}
 */


/*
    enable Nascom Monitor area at address 0
*/
void EnableNascomMonitor(){

        // lock the monitor area in rampage table so cannot be paged assume 1k so 2 entries
        ramlocktable[0] = 1;
        ramlocktable[1] = 1;
        // prevent write to this area 
        ramromtable[0] = 1; // DA N4 special 1 value means it will write to underlying default MAP80 memory
        ramromtable[1] = 1;
        // point those ram locations in rampagetable to Nascom Monitor 2k part of MonVWram
//        printf(" Monitor address %p \n",NascomMonVWram);
        for (int c=0; c< 2 ; ++c) {
            rampagetable[c]=NascomMonVWram+(c<<RAMPAGESHIFTBITS);
//            if (verbose){
//                printf("Nascom Mon - rampagetable %d address %p \n",c,rampagetable[c]);
//            }
        }
        if (nascompagedebug){
            printf("Nascom Mon - enabled rampagetable %d and %d \n",0,1);
        }
        // displayRamTable();

}
/*
    disable Nascom Monitor area at address 0
*/
void DisableNascomMonitor(){

        // unlock the monitor area in rampage table so can be paged assume 1k so 2 entries
        ramlocktable[0] = 0;
        ramlocktable[1] = 0;
        // enable writes to this area
        // has to be from ramromdefaulttable in case map80 ram pointing at dummy area
        ramromtable[0] = ramromdefaulttable[0]; 
        ramromtable[1] = ramromdefaulttable[1];
        // point those ram locations in rampagetable to ramdefaultpagetable ??
        for (int c=0; c< 2 ; ++c) {
            rampagetable[c]=ramdefaultpagetable[c];
//            if (verbose){
//                printf("Nascom Mon disable -  rampagetable %d address %p \n",c,rampagetable[c]);
//            }
        }
        if (nascompagedebug){
            printf("Nascom Mon disable - rampagetable %d and %d \n",0,1);
        }
        // displayRamTable();
}

/*
    enable Nascom work area at address $0C00.
*/
void EnableNascomWorkRam(){

    BYTE *WorkRamAddress = NascomMonVWram + 0xC00 ;
    int rampagetableentry=3;
        // lock the working ram area in rampage table so cannot be paged assume 1k 
        ramlocktable[rampagetableentry] = 1;
        // writes allowed to this area 
        ramromtable[rampagetableentry] = 0;
        // point those ram locations in rampagetable to Nascom Monitor 2k part of MonVWram
//        printf(" WorkRam address %p \n",WorkRamAddress);
        rampagetable[rampagetableentry]=WorkRamAddress;
        if (nascompagedebug){
            printf("Nascom Work Ram enabled - rampagetable %d \n",rampagetableentry);
        }
        // displayRamTable();
}
/*
    disable Nascom Monitor area at address $0C00.
*/
void DisableNascomWorkRam(){

    int rampagetableentry=3;
    // unlock the Work Ram area in rampage table so can be paged assume 1k 
    ramlocktable[rampagetableentry] = 0;
    // writes allowed to this area 
    // has to be from ramromdefaulttable in case map80 ram pointing at dummy area
    ramromtable[rampagetableentry] = ramromdefaulttable[rampagetableentry]; 
    // point those ram locations in rampagetable to ramdefaultpagetable 
    rampagetable[rampagetableentry]=ramdefaultpagetable[rampagetableentry];
    if (nascompagedebug){
        printf("Nascom Work Ram disabled - rampagetable %d \n",rampagetableentry);
    }
    // displayRamTable();
}

/*
    enable Nascom VideoRam area at address $
        $0800.
        $F800 (for NASCOM CP/M)..
    Do we pass over the address it should be loaded at
        or the entry in rampagetable 
        or check the NASVRAMHI of REMAP
        or just be passed NASVRAMHI of REMAP
    What if it is already enabled ?
     Do we check that or allow the calling code to check
      *  if value == 0 then address 0x800 else 0xF800 
*/
void EnableNascomVideoRam(unsigned char value){

    BYTE *VideoAddress = NascomMonVWram + 0x800 ;
    // can we have it at a high address ???
    // based on bit 2 from remap port
    // switched to using address shifted right to get entry number 
    // makes the code clearer - maybe :)
    int rampagetableentry=0x0800>>RAMPAGESHIFTBITS;;
    // if not 0 then uyse higher address
    if (value){
        rampagetableentry=0xF800>>RAMPAGESHIFTBITS;
    }
    // lock the working ram area in rampage table so cannot be paged assume 1k 
    ramlocktable[rampagetableentry] = 1;
    // writes allowed to this area 
    ramromtable[rampagetableentry] = 0;
    // point those ram locations in rampagetable to Nascom Monitor 2k part of MonVWram
    rampagetable[rampagetableentry]=VideoAddress;
    if (nascompagedebug){
        printf("Nascom Video ram  enabled - rampagetable %d \n",rampagetableentry);
    }
//    displayRamTable();

}
/*
    disable Nascom VideoRam area at address $0800.
         we can do a check on the which entry has the Video ram address
         * it is based on the value but . . . .
*/
void DisableNascomVideoRam(unsigned char value){

    BYTE *VideoAddress = NascomMonVWram + 0x800 ;
    int EntryNumber= -1;
    // hunt for the entry used for the video ram
    for (int c=0;c<RAMPAGETABLESIZE;++c){
        if (rampagetable[c] == VideoAddress){
            EntryNumber=c;
            break;
            }
        }
    // if found remove it
    if (EntryNumber > -1 ){
        // unlock the Video Ram area in rampage table so can be paged assume 1k 
        ramlocktable[EntryNumber] = 0;
        // writes allowed to this area 
        // has to be from ramromdefaulttable in case map80 ram pointing at dummy area
        ramromtable[EntryNumber] = ramromdefaulttable[EntryNumber]; 
        // point those ram locations in rampagetable to ramdefaultpagetable 
//        printf(" Monitor address %p \n",NascomMonVWram);
        rampagetable[EntryNumber]=ramdefaultpagetable[EntryNumber];
//            if (verbose){
//            printf("Nascom Video ram disabled - rampagetable %d address %p \n",c,rampagetable[c]);
//            }
        if (nascompagedebug){
            printf("Nascom Video ram disabled - rampagetable %d \n",EntryNumber);
        }
    }
//    displayRamTable();
}

/*
    enable SBROM at address 0x1000 

*/
void EnableSBROM(){

//    BYTE *SBROMAddress = SBROM ;
    BYTE *SBROMAddress = SBROM ;
        int rampagetableentry=4;
        if (rampagetable[rampagetableentry]!=SBROMAddress ){
            // lock the working ram area in rampage table so cannot be paged assume 1k 
            ramlocktable[rampagetableentry] = 1;
            // writes allowed to this area 
            ramromtable[rampagetableentry] = 1;  // DA N4 special 1 value means it will write to underlying default MAP80 memory
            // point those ram locations in rampagetable to Nascom Monitor 2k part of MonVWram
            // printf(" SBROM address %p \n",SBROMAddress);
            rampagetable[rampagetableentry]=SBROMAddress;

            if (nascompagedebug){
//                printf("SBROM enabled - rampagetable %d address %p \n",rampagetableentry,rampagetable[rampagetableentry]);
                printf("SBROM enabled - rampagetable %d \n",rampagetableentry);
            }
        }
//        displayRamTable();
}
/*
 *     disable SBROM 
 *  but we need to delay it till the next instruction
 * 
 */
void DisableSBROM(){

//    BYTE *SBROMAddress = SBROM ;
//    BYTE *SBROMAddress = NascomMonVWram + 0x1000;
    int rampagetableentry=4;

//    if (rampagetable[rampagetableentry] == SBROMAddress){
        // unlock the Work Ram area in rampage table so can be paged assume 1k 
        ramlocktable[rampagetableentry] = 0;
        // enable writes to this area 
        // has to be from ramromdefaulttable in case map80 ram pointing at dummy area
        ramromtable[rampagetableentry] = ramromdefaulttable[rampagetableentry]; 
        // point those ram locations in rampagetable to ramdefaultpagetable ??
        rampagetable[rampagetableentry]=ramdefaultpagetable[rampagetableentry];
        if (nascompagedebug){
            printf("SBROM Disabled -  rampagetable %d \n",rampagetableentry);
        }
//    }
//    displayRamTable();
}



// end of code
    
