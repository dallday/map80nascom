/*
 * defines how we handle the Nascom II PIO
 * 
 *  Port 04 - PIO port A data input and output
 *  Port 05  PIO port B data input and output
 *  Port 06  PIO port A control
 *  Port 07  PIO port B control
 * 
 */

#include <stdlib.h>            // std libraries
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "options.h"           //defines the options to use map80RamIntialise
#include "simz80.h"            // define all the z80 simulator
#include "map80VFCfloppy.h"    // define the floppy data stuff
#include "utilities.h"          // some useful bits of code
#include "map80nascom.h"
#include "statusdisplay.h"
#include "pio.h"
#include "tcpcomms.h"


// this is a array structure to define both port a and port b
PIOchip PIOPorts [2]={0};


/* called to reset the PIO state 
 * as per the datasheet
 * hopefully :)
 */
 
void PIOreset(){
     
    for (int portno=0;portno<2;portno++){
        
        PIOPorts[portno].Portmode=PIONONE;
        PIOPorts[portno].Portdata=0;
        PIOPorts[portno].Portsecondbyteexpected=0; // set to 1 if expect a second byte
        PIOPorts[portno].Portrdy=0;      // active high when data ready (output) or needed (input)
        PIOPorts[portno].Portstb=1;      // active low pulse to say data collected (output) or provided (input)
        PIOPorts[portno].PortInterruptAllowed=0; // set to 1 if allowed sett interrupt control word or interrupt disable word
        PIOPorts[portno].Portint=1;      // set low if interrupt requested but only if allowed 
    }
}

void PIOtotalreset(){
 
    PIOreset();

    for (int portno=0;portno<2;portno++){
        
        PIOPorts[portno].Portcontrol=0;
        PIOPorts[portno].Portintvector=0;
        PIOPorts[portno].Portandor=0;
        PIOPorts[portno].Porthighlow=0;
        PIOPorts[portno].Portintmask=0;
        PIOPorts[portno].Portiomask=0;
        PIOPorts[portno].Portlastintvalue=0xFF;  //should stop interrupt on first call ??
    }
}

/*
 * set the port mode based in the control data
 * return 0 if all okay or 1 if expecting second byte
 * 
 */
int PIO_Set_Mode(PIOPortSelect PIOPort,unsigned char value){

    // todo - check current state ????
    int retval=0;
    unsigned char mode1 = ((value & 0xC0)>>6 );
    
        
    if (PIOPort==PIOPORTA){
        // set port mode 
        PIOPorts[PIOPort].Portmode = mode1;
    } else {
        if (mode1 == 2){
            // not allowed for Port B
            fprintf(stderr,"Port B cannot be bidirectional ignored");
        }else {
            // set port mode 
            PIOPorts[PIOPort].Portmode = mode1;
        }
    }
    if (mode1==3){ // only if in mode 3 expect the io mask
                    // set to 1 means input set to 0 means output
                    // During Mode 3 operation, the strode signal is ignored and the Ready line is held Low. 
        retval=1;
    }

    return retval;
}

// TODO - handle control mode and bidirectional mode
void PIO_Porta_data_out(unsigned char value){

    PIO_Port_Data_out_common(PIOPORTA,value);

}

void PIO_Portb_data_out(unsigned char value){
    PIO_Port_Data_out_common(PIOPORTB,value);
}

void PIO_Port_Data_out_common(PIOPortSelect PortNo,unsigned char value) {

    switch (PIOPorts[PortNo].Portmode) {
    case PIOIN:
        // as it is input then this has no effect
        break;
    case PIOOUT:
        // putting data onto the port in output mode sets the rdy line high
        PIOPorts[PortNo].Portrdy=1;
        PIOPorts[PortNo].Portdata=value;
        if (PortNo==0){
            send_data_to_all_clients("AR\n");
        } else {
            send_data_to_all_clients("BR\n");
        }
        break;
    case PIOBIDIRECTIONAL:
        // TODO need input and output stores ??
        // can only be if Port A ??
        PIOPorts[PortNo].Portdata=value;
        break;
    case PIOCONTROL:
        // use the bit mask to decide which bits are set.
        int   binarybit=0;  // used to set the binary bit 
        unsigned char newportvalue=PIOPorts[PortNo].Portdata;
        unsigned int mask=0x80;
        // for each bit in the PIO port
        // TODO there should be a better way of doing this ?
        mask=0x80; // start with the top most bit
        for (binarybit=0;binarybit<8;binarybit++){
            if (!(PIOPorts[PortNo].Portiomask & mask)){
                // bit set as output
                if (value & mask ){
                    // set to 1
                    newportvalue |= mask;
                } else {
                    // set to 0 using xor to create the invert of the mask
                    newportvalue &= (0xFF ^ mask);
                }
            }
            mask=mask>>1; // move mask on 1 bit
        }
        if (newportvalue != PIOPorts[PortNo].Portdata){
            char line[64];
            snprintf(line, sizeof(line), "C%c %2.2X\n",PortNo==PIOPORTA ? 'A':'B',newportvalue);
            send_data_to_all_clients(line);
        }
        PIOPorts[PortNo].Portdata=newportvalue;
        // TODO check for interrupt
        PIO_checkcontrolInterrupt(PortNo);
        break;
    default:
        // not set do nothing
        break;
    }
    displayPIOportAlines(PortNo);
    printf("CPU write Port  %c value %2.2X port now %2.2X\n", PortNo==PIOPORTA ? 'A':'B',value, PIOPorts[PortNo].Portdata);

}

/*
 * checks current data value again the mask and high/low - and/or 
 * 
 * produces a value that is the current state of the interrupt lines
 * then compares it with last times before raising an interrupt
 * 
 */ 
void PIO_checkcontrolInterrupt(PIOPortSelect PortNo){

    unsigned char PIOmask=PIOPorts[PortNo].Portintmask;
    unsigned char PIOdata=PIOPorts[PortNo].Portdata;
    unsigned char step1;
    unsigned char step2; 
    if (PIOPorts[PortNo].PortInterruptAllowed){
        if (PIOPorts[PortNo].Porthighlow==0){
            // looking for low values on line 
            // invert the data
            PIOdata=~PIOdata;
        }
        // now or with data mask
        step1 = (PIOdata | PIOmask);
        if (PIOPorts[PortNo].Portandor == 1){
            // doing an and on the masked lines
            if (step1 == 0xFF){
                if (step1 != PIOPorts[PortNo].Portlastintvalue){
                    // new change to line 
                    PIOPorts[PortNo].Portint=0;
                }
            }
            printf("Port %c interrupt %u using AND value %2.2X  previous value %2.2X \n", 
                PortNo==PIOPORTA ? 'A':'B',PIOPorts[PortNo].Portint,step1,PIOPorts[PortNo].Portlastintvalue);
            PIOPorts[PortNo].Portlastintvalue=step1;
        }else{
            // looking for any lines high (or)
            PIOmask=~PIOmask;
            step2=step1 & PIOmask;
            if (step2 > 0){
                if (step2 != PIOPorts[PortNo].Portlastintvalue){
                    // new change to line 
                    PIOPorts[PortNo].Portint=0;
                }
            }
                printf("Port %c interrupt %u using OR value %2.2X  previous value %2.2X \n", 
                PortNo==PIOPORTA ? 'A':'B',PIOPorts[PortNo].Portint,step1,PIOPorts[PortNo].Portlastintvalue);
            PIOPorts[PortNo].Portlastintvalue=step2;
        }
    }
    
}




void PIO_Porta_control_out(unsigned char value){
    PIOPortSelect PortNo=PIOPORTA;
    PIO_control_out_common(PortNo,value);
}
 
/*
 * There are multiple control words
 *   mmxx1111 - set port mode mm is 0 1 2 or 3
 *              mode 3 has a i/o control byte ( 0 output , 1 input )
 *   vvvvvvv0  - identifies the interrupt vector (vvvvvvv0) 
 *   iahm0111   - enable/disable interrupts ( i=1 enable )
 *                  for mode 3 only
 *                  a = and/or the mask bits
 *                  h = active high or low
 *                  for all modes 
 *                  m = 1 - mask follows as next byte
 *                      only 0 in the next byte will be monitored
 *   ixxx0011   - enable/disable interrupts ( i=1 enable )
 *                but keaves the rest alone
 * 
 * 
 */

void PIO_control_out_common(PIOPortSelect PortNo,unsigned char value){
    
    int retval=0;
    // check if we were expecting a second byte for the port
    if (PIOPorts[PortNo].Portsecondbyteexpected){
        if (PIOPorts[PortNo].Portsecondbyteexpected==1){
            // save IO mask for mode 3 control
            PIOPorts[PortNo].Portiomask=value;
        } else {
            // save then interupt mask
            PIOPorts[PortNo].Portintmask=value;
        }
        PIOPorts[PortNo].Portsecondbyteexpected=0; // reset second byte expected
    } else {
        // nope - now check what control word has been sent
        PIOPorts[PortNo].Portcontrol=value;
        if ((value & 0x0F) == 0x0F){
            // se we have operation mode control word
            // set the mode and see if a second byte is to be expected
            retval=PIO_Set_Mode(PortNo,value);
            if (retval==0){
                PIOPorts[PortNo].Portsecondbyteexpected=0;
            }else {
                PIOPorts[PortNo].Portsecondbyteexpected=retval;
            }
        } else 
            // check for the vector control word bit 0 is 0
            if ((value &0x01) == 0 ) {
                    PIOPorts[PortNo].Portintvector=value;
            
        } else 
            // check for the interrupt enable control word
            if ((value &0x0F) == 0x07 ) {
                // interupt vector setting
                // bit 7 enables interupt ( 1 to enable )
                // bit 6 selects either and (1) or or (0)
                // bit 5 selects high (1) or low (0)
                // bit 4 set if int mask follows 
                    PIOPorts[PortNo].PortInterruptAllowed=((value & 0x80)==0x80);
                    PIOPorts[PortNo].Portandor=((value & 0x40)==0x40);
                    PIOPorts[PortNo].Porthighlow=((value & 0x20)==0x20);
                    if ((value &0x10) == 0x10){
                        PIOPorts[PortNo].Portsecondbyteexpected=2; // int mask follows
                        printf("Interrupt mask expected\n");
                    }
                    // TODO need to set Portlastintvalue so it does not trigger straight away 
                    // or maybe will
                    PIOPorts[PortNo].Portlastintvalue=0x00;

                    printf("interrupts allowed %2.2X\n", PIOPorts[PortNo].PortInterruptAllowed);
                    printf("and or set to      %2.2X\n", PIOPorts[PortNo].Portandor);
                    printf("high low set to    %2.2X\n", PIOPorts[PortNo].Porthighlow);

    
            }
            
    
        }

    displayPIOportAlines(PortNo);
    
}

void PIO_Portb_control_out(unsigned char value){
    PIOPortSelect PortNo=PIOPORTB;
    PIO_control_out_common(PortNo,value);
    
}


int PIO_Porta_data_in(){
    int retval=0;
    
    if ( PIOPorts[PIOPORTA].Portmode == PIONONE ){
        retval= 0x00; // not operational
    } else {
        if ( PIOPorts[PIOPORTA].Portmode == PIOIN ){
            PIOPorts[PIOPORTA].Portrdy=1;
            send_data_to_all_clients("AR\n");
        }
        retval= PIOPorts[PIOPORTA].Portdata;
    }
    displayPIOportAlines(PIOPORTA);


    return retval;
}

int PIO_Portb_data_in(){
    int retval=0;
    if ( PIOPorts[PIOPORTB].Portmode == PIONONE ){
        retval=0x00; // not operational
    } else {
        if ( PIOPorts[PIOPORTB].Portmode == PIOIN ){
            PIOPorts[PIOPORTB].Portrdy=1;
            send_data_to_all_clients("BR\n");
        }
        retval=PIOPorts[PIOPORTB].Portdata;
    }
    displayPIOportAlines(PIOPORTB);

    return retval;
}

int PIO_Porta_control_in(){
    // the control ports are  not readable
   //return Pio_portAcontrol;
    return 0xFF;
}
int PIO_Portb_control_in(){
    // the control ports are  not readable
    //return Pio_portBcontrol;
    return 0xFF;
}

#define Pio_portA_display_x 1
#define Pio_portB_display_x 20
#define Pio_portA_display_y 11
#define Pio_portB_display_y 11


void displayPIOoutline(){

    status_display_show_chars("CPU",15,17);

displayPIObasic(PIOPORTA);
displayPIObasic(PIOPORTB);
//displayPIOin(PIOPORTA);
//displayPIOout(PIOPORTB);
displayPIOportAlines(PIOPORTA);
displayPIOportAlines(PIOPORTB);

}
 
void displayPIObasic(PIOPortSelect PIOPort){   

    char line1[40];  
//    PIOPortMode PioPortMode = Pio_portBmode;
    int posx=Pio_portB_display_x;
    int posy=Pio_portB_display_y;
    if(PIOPort==PIOPORTA){
        posx=Pio_portA_display_x;
        posy=Pio_portA_display_y;
//        PioPortMode = Pio_portAmode;
    }


    // top line 
    line1[0]=0x90;  
    for (int pos1=1;pos1<9;pos1++){
        line1[pos1]=0x99;
    }
    line1[9]=0x98;
    line1[10]=0x98;
    line1[11]=0x99;
    line1[12]=0x98;
    line1[13]=0x91;
    line1[14]=0x0;
    status_display_show_chars(line1,posx,posy+1);
    // vertical line at each end and spaces in middle
    line1[0]=0x94; //
    for (int pos1=1;pos1<13;pos1++){
        line1[pos1]=0x20;
    }
    line1[13]=0x94;
    line1[14]=0x0;
    status_display_show_chars(line1,posx,posy+2);
    status_display_show_chars(line1,posx,posy+3);
    // bottom line
    line1[0]=0x92;
    for (int pos1=1;pos1<9;pos1++){
        line1[pos1]=0x9A;
    }
    line1[9]=0x98;
    line1[10]=0x98;
    line1[11]=0x9A;
    line1[12]=0x98;
    line1[13]=0x93;
    line1[14]=0x0;
    status_display_show_chars(line1,posx,posy+4);
}

/*
 * displays "arrows" showing out
 * TODO - check port data 
 * and set colour of each dataline 
 * display value (hex) in center of box
 * check control data for direction 
 * 
 * 
*/

void displayPIOportAlines(PIOPortSelect PortNo){
    
    int posx=Pio_portB_display_x;
    int posy=Pio_portB_display_y;
    if (PortNo==PIOPORTA){
        posx=Pio_portA_display_x;
        posy=Pio_portA_display_y;
    }

    PIOPortMode PioPortMode = PIOPorts[PortNo].Portmode;


    unsigned char dataValue=PIOPorts[PortNo].Portdata;
    unsigned char iovalue=PIOPorts[PortNo].Portiomask;

    char  stemp[10];      // temp.String für sprintf()
    char  binaryvalue[9]; // set to the binary value 8 plus 0
    char  binarymask[9]; // set to the binary value 8 plus 0
    int   binarybit=0;  // used to set the binary bit in above
    unsigned int mask=0x01;
    binaryvalue[8]=0; // set end marker
    binarymask[8]=0; // set to the binary value 8 plus 0


// display the top lines which connect to the outside world
    unsigned char connectortop = 0xB2; //  arrow down 
    unsigned char connectorbottom = 0xB2; //  arrow down 
    uint32_t charcolour;
    int bitposx = posx + 1 ;  // these markers start in column 2
    // for each bit in the PIO port
    mask=0x80; // start with the top most bit
    for (binarybit=0;binarybit<8;binarybit++){
        if (dataValue & mask) {
            // bit is 1
            charcolour=STATUS_COLOR_RED;
            binaryvalue[binarybit]='1';
        } else {
            charcolour=STATUS_COLOR_BLUE;
            binaryvalue[binarybit]='0';
        }

        switch (PioPortMode) {
            case PIOIN:
                connectortop = 0xB2; //  arrow Down
                connectorbottom = 0xB2; //  arrow down
                break;
            case PIOOUT:
                connectortop = 0xB3; //  arrow up
                connectorbottom = 0xB3; //  arrow up 
                break;
            case PIOBIDIRECTIONAL:
                connectortop = 0xB2; //  arrow down
                connectorbottom = 0xB2; //  arrow down 
                break;
            case PIOCONTROL:
                if (iovalue & mask ){  // if mask is 1 then input
                    connectortop = 0xB2; //  arrow Down
                    connectorbottom = 0xB2; //  arrow down
                    binarymask[binarybit]='1';
                } else {
                    connectortop = 0xB3; //  arrow up
                    connectorbottom = 0xB3; //  arrow up 
                    binarymask[binarybit]='0';
                }
                
                break;
            case PIONONE:
                connectortop = 0x20; //  blank as not yet set
                connectorbottom = 0x20; //  blank as not yet set
                break;
            default:
                break;
        }
        status_display_show_char_full(connectortop,bitposx,posy,charcolour,STATUS_COLOR_BACKGROUND);
        status_display_show_char_full(connectorbottom,bitposx,posy+5,charcolour,STATUS_COLOR_BACKGROUND);
        bitposx++; // step on 1 character
        mask=mask>>1;
    }    
        // clear  INT info
        status_display_show_chars_full("   ",posx+10,posy+3,charcolour,STATUS_COLOR_BACKGROUND);
        status_display_show_chars_full(" ",posx+11,posy+5,charcolour,STATUS_COLOR_BACKGROUND);
        // clear  rdy info
        status_display_show_chars_full("   ",posx+10,posy+2,charcolour,STATUS_COLOR_BACKGROUND);
        status_display_show_chars_full(" ",posx+11,posy,charcolour,STATUS_COLOR_BACKGROUND);
        // clear mask line 
        status_display_show_chars_full("       ",posx+2,posy-2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
        status_display_show_chars_full("        ",posx+1,posy-1,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);

    if (PioPortMode!=PIONONE) {
        //status_display_show_char_full(connectortop,posx,posy,charcolour,STATUS_BACKGROUND);
        sprintf(stemp,"0X%2.2X",dataValue);
        // display the hex value of the data
        status_display_show_chars_full(stemp,posx+3,posy+2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
        // display the binay value of the data
        status_display_show_chars_full(binaryvalue,posx+1,posy+3,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
        if (PioPortMode==PIOCONTROL){
            // display mask bit
            // posx has been incremented
            status_display_show_chars_full("IO Mask",posx+2,posy-2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
            status_display_show_chars_full(binarymask,posx+1,posy-1,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
        } else {
            
            if (PIOPorts[PortNo].Portrdy==1) {
                // bit is 1
                charcolour=STATUS_COLOR_RED;
                binaryvalue[0]='1';
            } else {
                charcolour=STATUS_COLOR_BLUE;
                binaryvalue[0]='0';
            }
            status_display_show_chars_full("RDY",posx+10,posy+2,charcolour,STATUS_COLOR_BACKGROUND);
            status_display_show_char_full(binaryvalue[0],posx+11,posy,charcolour,STATUS_COLOR_BACKGROUND);
            if (PIOPorts[PortNo].PortInterruptAllowed==1){
                if (PIOPorts[PortNo].Portint==0) {
                    // bit is 1
                    charcolour=STATUS_COLOR_RED;
                    binaryvalue[0]='1';
                } else {
                    charcolour=STATUS_COLOR_BLUE;
                    binaryvalue[0]='0';
                }
                status_display_show_chars_full("Int",posx+10,posy+3,charcolour,STATUS_COLOR_BACKGROUND);
                status_display_show_char_full(binaryvalue[0],posx+11,posy+5,charcolour,STATUS_COLOR_BACKGROUND);
            }
            
        }
    }

}
    
/*
 * called at the start of each instruction cycle
 * and passes in the IEI which was the IEO from the previous device
 * and gets back the IEO from this device
 * The first device will get 1 in the IEI
 * if the device does not need to raise an interrupt the IEO will be 1
 * if the device want to raise an maskable interrupt it returns 0
 * 
 * TODO - need to handle IEI and IEO correctly for multiples
 * and reset stuff when the RETI happens for this request
 * -- currently the ack cleasrs the int 
 * 
 * This routine will do any stuff the PIO needs
 * and check if interrupt is requested
 * 
 */
int PIOstatuscheck(int IEI) {
    
    int IEO=IEI;

    if (IEI > 0 ) {
        // nothing has more priority in the int chain
        // check if interrupt has been requested 
        // Port A first and then Port B
        if (PIOPorts[PIOPORTA].Portint==0){
            IEO=0;
        }
        if (PIOPorts[PIOPORTB].Portint==0){
            IEO=0;
        }
    }
    
    return IEO;
   
}
/*
 * called when the main cycle in simz80 says it can handle a maskable interrupt
 * 
 * it returns the interrupt vectore address 
 * 
 * if the request was made byt this port this will set the int line high 
 * but needs to keep the IEO line low until we do a RETI
 * 
 * 
 * 
 */

WORD PIOInterruptAcknowledge(){

    WORD retvalue=0;
    if (PIOPorts[PIOPORTA].Portint==0){
        PIOPorts[PIOPORTA].Portint=1; // reset interrupt request
        printf("interrupt acknowledge Port A %4.4X\n", PIOPorts[PIOPORTA].Portintvector);
        retvalue=PIOPorts[PIOPORTA].Portintvector;
        displayPIOportAlines(PIOPORTA);
    }
    if (PIOPorts[PIOPORTB].Portint==0){
        PIOPorts[PIOPORTB].Portint=1;  // reset interrupt request
        printf("interrupt acknowledge Port B %4.4X\n", PIOPorts[PIOPORTB].Portintvector);
        retvalue=PIOPorts[PIOPORTB].Portintvector;
        displayPIOportAlines(PIOPORTB);
    }
    // if no interrupt requested 
    return retvalue;
    
}




/*
 * pass the data from port to the device reading it
 * for mode Output return the data 
 *    Need a stobe signal to check if need an interrupt
 *    and reset (0) the ready signal. 
 * for mode Input return the data 
 * 
 * for mode control 
 *  returns the port data
 * 
 * for mode bidirectional return data (TODO)
 * 
 * whatever mode just return the data
 */ 
 unsigned char PIODeviceReadPort(PIOPortSelect PortNo){


    printf("Device read Port  %c value %2.2X \n", PortNo==PIOPORTA ? 'A':'B',PIOPorts[PortNo].Portdata);
    unsigned char retvalue=0;
    // on read we should also strobe the port line to say byte read
    // this may generate an int and set rdy line low 
    switch (PIOPorts[PortNo].Portmode) {
    case PIOIN:
        // return the value in the port 
        // - although as input mode the device should be writing to the port
        retvalue=PIOPorts[PortNo].Portdata;
        break;
    case PIOOUT:
        // return the value in the port 
        // as the port is set to output mode this will trigger an interrupt if allowed
        // and set the ready line to low
        PIOPorts[PortNo].Portrdy=0;
        if (PIOPorts[PortNo].PortInterruptAllowed){
            PIOPorts[PortNo].Portint=0;  // activate an interrupt
        }
        retvalue=PIOPorts[PortNo].Portdata;
        break;
    case PIOBIDIRECTIONAL:
        // TODO need input and output stores ??
        // ignore for now
        break;
    case PIOCONTROL:
        // return the value in the port 
        // - although as input mode the device should be writing to the port
        retvalue=PIOPorts[PortNo].Portdata;
        break;
    default:
        // not set do nothing
        break;
    }

    displayPIOportAlines(PortNo);

    return retvalue;

    
}

/* store data from the device to the PIO port
 * 
 * for mode Output dont store the data 
 * 
 * for mode Input store the data 
 *  need strobe signal for interrupt
 * 
 * for mode control 
 *  store only the output bits of data to the port data
 * 
 * for mode bidirectional store data (TODO)
 * 
 */ 

unsigned char PIODeviceWritePort(PIOPortSelect PortNo, unsigned char value){

    
    switch (PIOPorts[PortNo].Portmode) {
    case PIOIN:
        // a write to the port when in input mode
        // should trigger the interrupt if allowed 
        // and set the rady flag to 0
        PIOPorts[PortNo].Portrdy=0;
        if (PIOPorts[PortNo].PortInterruptAllowed){
            PIOPorts[PortNo].Portint=0;  // activate an interrupt
        }
        PIOPorts[PortNo].Portdata=value;
        break;
    case PIOOUT:
        // ignore data as port is in output mode
        break;
    case PIOBIDIRECTIONAL:
        // TODO need input and output stores ??
        PIOPorts[PortNo].Portdata=value;
        break;
    case PIOCONTROL:
        // use the bit mask to decide which bits are set.
        int   binarybit=0;  // used to set the binary bit 

        unsigned int mask=0x80;
        // for each bit in the PIO port
        // TODO there should be a better way of doing this ?
        mask=0x80; // start with the top most bit
        for (binarybit=0;binarybit<8;binarybit++){
            if ((PIOPorts[PortNo].Portiomask & mask)){
                // bit set as input
                if (value & mask ){
                    // set to 1
                    PIOPorts[PortNo].Portdata |= mask;
                } else {
                    // set to 0 using xor to create the invert of the mask
                    PIOPorts[PortNo].Portdata &= (0xFF ^ mask);
                }
            }
            mask=mask>>1; // move mask on 1 bit
        }
        // TODO check for interrupt
        PIO_checkcontrolInterrupt(PortNo);
        break;
    default:
        // not set do nothing
        break;
    }
    printf("Device write Port  %c value %2.2X port now %2.2X\n", PortNo==PIOPORTA ? 'A':'B',value, PIOPorts[PortNo].Portdata);
    displayPIOportAlines(PortNo);

    return value;
    
}

/*
 * read the state of the ports ready line
 */
unsigned char PIOReadReadyLine(PIOPortSelect PortNo){

    printf("Device read Port %c ready line %2.2X \n", PortNo==PIOPORTA ? 'A':'B',PIOPorts[PortNo].Portrdy);
 
 
    return PIOPorts[PortNo].Portrdy;
    
}

/*
 * pulse the stb line for the port 
 * 
 * sort of goes low then high again
    TODO should it be 2 calls 1 to set low and 1 to set high 
 * decided to not use this at all see device read and write 
 * 
 */
unsigned char PIOStrobeLine(PIOPortSelect PortNo){

        switch (PIOPorts[PortNo].Portmode) {
        case PIOIN:
            PIOPorts[PortNo].Portrdy=0;
            if (PIOPorts[PortNo].PortInterruptAllowed){
                PIOPorts[PortNo].Portint=0;  // activate an interrupt
            }
            break;
        case PIOOUT:
            PIOPorts[PortNo].Portrdy=0;
            if (PIOPorts[PortNo].PortInterruptAllowed){
                PIOPorts[PortNo].Portint=0;  // activate an interrupt
            }
            break;
        case PIOBIDIRECTIONAL:
            // TODO need input and output stores ??
            // 
            break;
        case PIOCONTROL:
            // no action
            break;
        case PIONONE:
            // not set do nothing
            break;
        default:
            // not set do nothing
            break;
        }
        
    printf("Device strobe Port  %c - port int %d\n", PortNo==PIOPORTA ? 'A':'B', PIOPorts[PortNo].Portint);
    return 0;
}

