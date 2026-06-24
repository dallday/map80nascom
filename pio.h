/*
 * definitions for the Nascom II PIO 
 * 
 *  Port 04 - PIO port A data input and output
 *  Port 05  PIO port B data input and output
 *  Port 06  PIO port A control
 *  Port 07  PIO port B control

 */

typedef enum { PIOPORTA = 0, PIOPORTB = 1 } PIOPortSelect;
typedef enum { PIOOUT = 0 , PIOIN =1, PIOBIDIRECTIONAL = 2, PIOCONTROL =3 , PIONONE = 4} PIOPortMode;

// the routines to handle it 
void PIO_Porta_data_out(unsigned char value);
void PIO_Portb_data_out(unsigned char value);
void PIO_Porta_control_out(unsigned char value);
void PIO_Portb_control_out(unsigned char value);


int PIO_Porta_data_in();
int PIO_Portb_data_in();
int PIO_Porta_control_in();
int PIO_Portb_control_in();


void displayPIOoutline();
void displayPIObasic(PIOPortSelect PIOPort);
//void displayPIOin(PIOPortSelect PIOPort);
//void displayPIOout(PIOPortSelect PIOPort);
void displayPIOportAlines(PIOPortSelect PIOPort);
    
extern unsigned char Pio_portAdata;
extern unsigned char Pio_portBdata;
extern unsigned char Pio_portAcontrol;
extern unsigned char Pio_portBcontrol;

/* define characters to use 
 */




// TODO define how to handle interrupts 

