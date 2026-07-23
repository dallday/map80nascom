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

// thinking of using this structure TODO
typedef struct {
    PIOPortMode Portmode;
    unsigned char Portdata;
    unsigned char Portcontrol;
    unsigned char Portintvector;
    unsigned char Portandor;    // is it all lines or just 1 that is needed to fire an interrupt
    unsigned char Porthighlow;  // set to if we are looking for high or low in interrupt mask
    unsigned char Portintmask;  // control mode 3 - interrupt mask 0 means monitor
    unsigned char Portiomask;   // control mode 3 input (1) or output lines
    unsigned char Portlastintvalue; // holds the last interrupt value for control mode 3
    int Portsecondbyteexpected; // set to 1 if expect a second byte
    unsigned char Portrdy;      // active high when data ready (output) or needed (input)
    unsigned char Portstb;      // active low pulse to say data collected (output) or provided (input)
    unsigned char PortInterruptAllowed; // set to 1 if allowed set interrupt control word or interrupt disable word
    unsigned char PortIEI; // from last status call 
    unsigned char PortIEO; // from last status call 
    unsigned char PortInterruptBeingServiced;    // set to low if interrupt was being serviced waiting for RETI
    unsigned char Portint;      // set low if interrupt requested but only if allowed 
} PIOchip;


// the routines to handle it 
void PIO_Porta_data_out(unsigned char value);
void PIO_Portb_data_out(unsigned char value);
void PIO_Port_Data_out_common(PIOPortSelect PortNo,unsigned char value);
void PIO_checkcontrolInterrupt(PIOPortSelect PortNo);

void PIO_Porta_control_out(unsigned char value);
void PIO_Portb_control_out(unsigned char value);
void PIO_control_out_common(PIOPortSelect PortNo,unsigned char value);

int PIO_Porta_data_in();
int PIO_Portb_data_in();
// these actually do nothing 
int PIO_Porta_control_in();
int PIO_Portb_control_in();


void displayPIOoutline();
void displayPIObasic(PIOPortSelect PIOPort);
//void displayPIOin(PIOPortSelect PIOPort);
//void displayPIOout(PIOPortSelect PIOPort);
void displayPIOportAlines(PIOPortSelect PIOPort);

// interrupt handling routines
int PIOstatuscheck(int IEI);
WORD PIOInterruptAcknowledge(); 

// device connection routines
unsigned char PIODeviceReadPort(PIOPortSelect PortNo);
    
unsigned char PIODeviceWritePort(PIOPortSelect PortNo, unsigned char value);

unsigned char PIOReadReadyLine(PIOPortSelect PortNo);

unsigned char PIOStrobeLine(PIOPortSelect PortNo);




void PIOreset();
void PIOtotalreset();





/* define characters to use 
 */




// TODO define how to handle interrupts 

