/*
 * definitions for MAP80RAM
 */

// routines 
void map80RamInitialise();
void map80Ram(unsigned char value);
void displayRamTable();
void resetMemoryPages();

// external variables
// rams areas for map80 card
extern  int ramromtable[];
extern  int ramromdefaulttable[];

extern int ramlocktable[];
// DA N4 added rampagetable nope-  is in simz80.h
// extern BYTE *rampagetable[RAMPAGETABLESIZE];
extern BYTE *ramdefaultpagetable[RAMPAGETABLESIZE];

extern BYTE NascomMonVWram[];
extern BYTE dummyram[];
extern BYTE SBROM[];

extern BYTE vfcrom[];
extern BYTE vfcdisplayram[];
extern BYTE statusdisplayram[];


extern int rampagedebug;    // set to 1 to display rampage details



// end of file

