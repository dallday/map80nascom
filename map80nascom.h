#ifndef MAP80NASCOM_H
#define MAP80NASCOM_H 1

#define VERSION "8.1"
#define YAZEVERSION "1.10"

// #define INSCREEN(x) (((x) >> 10) == 2)

//#ifndef LIBDIR
//#define LIBDIR "/usr/local/lib/"
//#endif

#define SLOW_DELAY  25000
#define FAST_DELAY 900000

// define external procedures
extern int main(int argc, char **argv);
extern int setup(int, char **);
extern int sim_delay(void);

// these reutines are called by simz80 when in or out opcodes are called
extern void out(unsigned int port, unsigned char value);
extern int in(unsigned int port);

extern int verbose;      // set to true to display messages

extern bool go_fast;
extern int t_sim_delay;

extern int usebiosmonitor;

extern int traceon;     // set to 1 to trace z80

extern int tracestartaddress;    // trace will only show results when the PC is within this range
extern int traceendaddress;      //  end address of the trace range ( 0 to 0xFFFF )


/*
 * 1.7 Input/output port addressing
 *
 *     output on keyboard connector.
 *     Output Bit
 * P0  7 Not available          7 Unused
 *     6 Not used               6 Keyboard S6
 *     5 Unused                 5 Keyboard S3
 *     4 Tape drive LED         4 Keyboard S5
 *     3 Single step            3 Keyboard S4
 *     2 Unused                 2 Keyboard S0
 *     1 Reset keyb'd count     1 Keyboard S2
 *     0 Clock keyb'd count     0 Keyboard S1
 */

#define P0_OUT_TAPE_DRIVE_LED 16
#define P0_OUT_SINGLE_STEP     8
#define P0_OUT_KEYBOARD_RESET  2
#define P0_OUT_KEYBOARD_CLOCK  1

/*
 * P1  0 - 7 Data to UART       0 - 7 Data from UART
 *     (Serial port)            (Serial port)
 *
 * P2  0 - 7 Not assigned       7 Data received from UART
 *                              6 UART TBR empty
 *                              5 Not assigned
 *                              4 Not assigned
 *                              3 F error on UART
 *                              2 P error on UART
 *                              1 O error on UART
 *                              0 Not assigned
 */

#define UART_DATA_READY 128     // data ready to pickup
#define UART_TBR_EMPTY   64     // uart can accept data 
#define UART_F_ERROR      8     // error status 
#define UART_P_ERROR      4
#define UART_O_ERROR      2     

/*
 * P3  Not assigned             Not assigned
 *
 * P4  PIO port A data input and output
 *
 * P5  PIO port B data input and output
 *
 * P6  PIO port A control
 *
 * P7  PIO port B control
 */

#endif

// end of file
