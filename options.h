/* 
 *  define some of the options for map80Nascom emulator.
 * 
 *  Be careful changing any of the other definitions in the module
 *   as they may not work !!!!
 * 
 * 
 * 
*/

// check if already been defined if so don't define again
#ifndef options_h
#define options_h

// keyboard - 
// 0 = us keyboard
// 1 = uk keyboard
#define KEYBOARDTYPE 1



// define to use Memory Management unit
// #define MMU 1
// removed as always doing MMU
// makes coding easier

// Note see simz80.h for the defining of the actual ram 

// this defines how much memory the Z80 can see in K
#define RAMSIZE 64
// this defines how much space we allow for virtual memory available to use for MMU in K
#define VIRTUALRAMSIZE 1024
//#define VIRTUALRAMSIZE 2048

// sets the size of the pages used with the Memory Management unit
// used to set the mapping pagetable using MEMSIZE/MEMPAGESIZE
// replaced the original 4 hard coded in the original
// also makes it easier to protect monitor ROM
// and stop monitor rom, video and working ram from being swapped out
// DA N4 switched to 1k pages as N4 had seperate controls for 
// Vram and Wram in the area NascomMonVWram
#define RAMPAGESIZE (1)

// defines the ram_page table size
// 1 entry for each 2k block in the 64k memory
#define RAMPAGETABLESIZE (( RAMSIZE / RAMPAGESIZE ))

// we use a macro called RAM (see simz80.h)  which is 
// RAM(a)		*(rampagetable[((a)>>RAMPAGESHIFTBITS) & RAMPAGETABLESIZEMASK] + ((a) & RAMPAGEMASK) )
// which calculates the address of the byte in actual memory from the virtual address (a) 


// defines the ram_page table size mask
// so after the address shifted 11 bits to the right and with RAMPAGETABLESIZEMASK to ensure we don't overflow ram_page table (rampagetable)
#define RAMPAGETABLESIZEMASK (( RAMSIZE / RAMPAGESIZE ) -1 )

// for Memory Mamagement we need to know how many bits in the address define the entry in the ram_page table.
// for 2k pages in 64k we are only interested in the first 5 bits ( eg 0xF800 ) 
//     the lower 11 bits represet the off set ( eg 0x07FF ) ( 2 * 1024 -1 )
// we can shift the address over a number of bits to get the entry in the ram_pages table (rampagetable)
// so for 2k we need to shift 11 bits to the right 
// DA N4 we switched to 1k page sizes so 
// for 1k pages in 64k we are only interested in the first 6 bits ( eg 0xFC00 ) 
//     the lower 11 bits represet the off set ( eg 0x03FF ) ( 1 * 1024 -1 )
// we can shift the address over a number of bits to get the entry in the ram_pages table (rampagetable)
// so for 1k we need to shift 10 bits to the right 
#define RAMPAGESHIFTBITS (10)

// and then we need to know what bit of the address we need to use for the offset into the memory page
// for 2k that is 0x7FF ( 2 * 1024 -1 )
// DA N4 for 1k that is 0x3FF ( 1 * 1024 -1 )
#define RAMPAGEMASK (0x3FF)

// defines for the screen position of the 2 displays
// only 1 of the NASCOM or VFC is displayed 
// and the status one is displayed to it's right.
#define NASCOM_DISPLAY_XPOS (40)
#define NASCOM_DISPLAY_YPOS (40)

#define MAP80VFC_DISPLAY_XPOS (40)
#define MAP80VFC_DISPLAY_YPOS (40)

#define STATUS_DISPLAY_XPOS (40) 
#define STATUS_DISPLAY_YPOS (40)


// force a refresh on the screens - end if no updates
#define MAP80VFC_DISPLAYFORCEREFRESH (10)
#define NASCOM_DISPLAYFORCEREFRESH (10)

/*
 * Defined global vaiables etc here as make life simpler
 * DA N4 :)
 * 
 */

// these are to hold the values written to for N4 ports
extern unsigned int Port_REMAP_value;
extern unsigned int Port_PROTECT_value;
extern unsigned int Port_MWAITS_value;
extern unsigned int Port_PORPAGE_value;
extern unsigned int Port_REASON_value;
extern unsigned int Port_SERCON_value;

extern int N4portdebug;

// defines for the N4 ports
#define Port_REMAP   0x18
#define Port_PROTECT 0x19
#define Port_MWAITS  0x1A
#define Port_PORPAGE 0x1B
#define Port_REASON  0x1C
#define Port_SERCON  0x1D


// moved here so simz80 could see it
extern int usebiosmonitor;

// the actual variable is defined in map80nascom.c
// set to 1 to show debug details for the vfc display
extern int vfcdisplaydebug;
// #define VFCDISPLAYDEBUG 0   

// set to 1 to display floppy debug details
extern int vfcfloppydebug;
// set to 1 to display floppy sectors read and written
extern int  vfcfloppydisplaysectors;
// set to 1 to show the nascom keyboard matrix each time nassys does a keyboard scan.
// displayed when it does an index reset.
extern int  showkeymatrix;
// shows the sdl key values during processing
extern int  displaykeyvalues;
// display clock card processing
extern int chsclockcarddebug;

// display nascom paging in and out
extern int nascompagedebug;

#endif

