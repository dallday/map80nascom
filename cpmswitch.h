/*
 header file for cpmswitch.c
 
 */

// DA N4 no longer used 
// extern int cpmswitchstate;   // set to 1 if cpm mode set
                         // vfc boot rom in at 0 to start
                        // Nascom ROM and VWRAM not enabled
                       //
                       
// extern void setcpmswitch(int state); // set the mode for cpm switch 
 // 0 - NASSYS
 // 1 - cpm - mapvfc link 4 is 


extern void EnableNascomMonitor();
extern void EnableNascomWorkRam();
extern void EnableNascomVideoRam(unsigned char value);
extern void EnableSBROM();
 
extern void DisableNascomMonitor();
extern void DisableNascomWorkRam();
extern void DisableNascomVideoRam(unsigned char value);
extern void DisableSBROM();




extern int verbose;      // set to true to display messages
 

 
 // end of code
 