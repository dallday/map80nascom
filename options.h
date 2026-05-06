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

// set to 1 to show debug details for the vfc display
#define VFCDISPLAYDEBUG 0   

// set to 1 to display floppy debug details
#define VFCFLOPPYDEBUG 0
// set to 1 to display floppy sectors read and written
#define VFCFLOPPYDISPLAYSECTORS 0

// set to 1 to show the nascom keyboard matrix each time nassys does a keyboard scan.
// displayed when it does an index reset.
#define SHOWKEYMATRIX 0

// shows the sdl key values during processing
#define DISPLAYKEYVALUES 0

// display clock card processing
#define CHSCLOCKCARDDEBUG 0

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
#define RAMPAGESIZE (2)

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
#define RAMPAGESHIFTBITS (11)

// and then we need to know what bit of the address we need to use for the offset into the memory page
// for 2k that is 0x7FF ( 2 * 1024 -1 )
#define RAMPAGEMASK (0x7FF)

// defines for the screen position of the 2 displays
// only 1 of the NASCOM or VFC is displayed 
// and the status one is displayed to it's right.
#define NASCOM_DISPLAY_XPOS (40)
#define NASCOM_DISPLAY_YPOS (40)

#define MAP80VFC_DISPLAY_XPOS (40)
#define MAP80VFC_DISPLAY_YPOS (40)

#define STATUS_DISPLAY_XPOS (100*8) // not used really !!!!
#define STATUS_DISPLAY_YPOS (40)


// force a refresh on the screens - end if no updates
#define MAP80VFC_DISPLAYFORCEREFRESH (10)
#define NASCOM_DISPLAYFORCEREFRESH (10)

#endif
