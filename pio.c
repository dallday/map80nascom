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

 * a reset does NOT clear the interrupt vector address
 */
 
void PIOreset(){
     
    for (int portno=0;portno<2;portno++){
        
        PIOPorts[portno].Portmode=PIONONE;
        PIOPorts[portno].Portdata=0;
        PIOPorts[portno].Portsecondbyteexpected=0; // set to 1 if expect a second byte
        PIOPorts[portno].Portrdy=0;      // active high when data ready (output) or needed (input)
        // PIOPorts[portno].Portstb=1;      // active low pulse to say data collected (output) or provided (input)
        PIOPorts[portno].PortInterruptAllowed=0; // set to 1 if allowed to genetate an interrupt request
        PIOPorts[portno].Portint=0;      // set low if interrupt requested but only if allowed 
        PIOPorts[portno].PortInterruptBeingServiced=0;  // no interrupt beeing processed 
        PIOPorts[portno].Portcontrol=0;
        PIOPorts[portno].Portandor=0;
        PIOPorts[portno].Porthighlow=0;
        PIOPorts[portno].Portintmask=0;
        PIOPorts[portno].Portiomask=0;
        PIOPorts[portno].Portlastintvalue=0xFF;  //should stop interrupt on first call ??
        displayPIOportAlines(portno);
        //displayPIOportAlines(PIOPORTB);
    }
}

/* 
 * a full reset clears the interrupt vector address
 */
void PIOtotalreset(){


    for (int portno=0;portno<2;portno++){
        PIOPorts[portno].Portintvector=0;
    }
    PIOreset();

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
        if (PIOdebug) printf("Port %c - IO mask expected\n",PIOPort==PIOPORTA ? 'A':'B');

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
            send_data_to_all_clients("AR");
        } else {
            send_data_to_all_clients("BR");
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
            snprintf(line, sizeof(line), "C%c %2.2X",PortNo==PIOPORTA ? 'A':'B',newportvalue);
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
    if (PIOdebug) {
        printf("CPU write Port  %c value %2.2X port now %2.2X\n", 
            PortNo==PIOPORTA ? 'A':'B',value, PIOPorts[PortNo].Portdata);
    }

}

/*
 * checks current data value again the mask and high/low - and/or 
 * 
 * produces a value that is the current state of the interrupt lines
 * then compares it with last times before raising an interrupt
 * 
 * found that if looking for high values on the data lines
 * If the condition was all lines (AND) and high
 *  since the int mask has 0 for active int and 1 for not active
 *  if all the active data lines were 1, and all the non interrupt lines 1 in the Int mask
 *  using or we will be 0xFF if all the interrupt lines are set.
 * 
 * If the condition is any line (OR) and high
 *  after the OR of the data and Interrupt mask
 *  I then invert the Interrupt mask and and it with the result of the OR
 *  If the results is > 0 then omne of the lines is high
 *  again need to check if this is a new condition or not.
 * 
 * If the condition is for low values 
 *   inverting the data lines at the start works.
 * 
 * Note: - although the conditions may exist to set the int flag
 *          it will only happen if interrupt mode is set and IEI is high :)
 * 
 * 
 */ 
void PIO_checkcontrolInterrupt(PIOPortSelect PortNo){

    unsigned char PIOintmask=PIOPorts[PortNo].Portintmask;
    unsigned char PIOdata=PIOPorts[PortNo].Portdata;
    unsigned char step1;
    unsigned char step2; 
    if (PIOPorts[PortNo].PortInterruptAllowed){
        if (PIOPorts[PortNo].Porthighlow==0){
            // looking for low values on line 
            // invert the data
            PIOdata=~PIOdata;
        }
        // now or data with the interrupt mask
        step1 = (PIOdata | PIOintmask);
        if (PIOPorts[PortNo].Portandor == 1){
            // we need all the required lines to be HIGH 
            // since the int mask has 0 for active int and 1 for not active
            // if all the active data lines were 1, and all the non interrupt lines 1 in the Int mask
            // using or we will be 0xFF if all the interrupt lines are set
            // 
            if (step1 == 0xFF){
                // but we need to check if this is a change from the last check
                // interrupts only get generated on change.
                if (step1 != PIOPorts[PortNo].Portlastintvalue){
                    // new change to line - set to say interrupt can be generated
                    PIOPorts[PortNo].Portint=1;
                }
            }
            if (PIOdebug) {
                // this tells me if the interrupt state has changed.
                printf("Port %c interrupt %u using AND value %2.2X  previous value %2.2X \n", 
                    PortNo==PIOPORTA ? 'A':'B',PIOPorts[PortNo].Portint,step1,PIOPorts[PortNo].Portlastintvalue);
            }
            PIOPorts[PortNo].Portlastintvalue=step1;
        }else{
            // looking for any lines high (or)
            PIOintmask=~PIOintmask;
            step2=step1 & PIOintmask;
            if (step2 > 0){
                if (step2 != PIOPorts[PortNo].Portlastintvalue){
                    // new change to line 
                    PIOPorts[PortNo].Portint=1;
                }
            }
            if (PIOdebug) {
                // this tells me if the interrupt state has changed.
                printf("Port %c interrupt %u using OR value %2.2X  previous value %2.2X \n", 
                    PortNo==PIOPORTA ? 'A':'B',PIOPorts[PortNo].Portint,step1,PIOPorts[PortNo].Portlastintvalue);
            }
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
                    if (PIOdebug) printf("Port %c - Interrupt mask expected\n",PortNo==PIOPORTA ? 'A':'B');
                }
                // TODO need to set Portlastintvalue so it does not trigger straight away 
                // or maybe will
                PIOPorts[PortNo].Portlastintvalue=0x00;
                reportportstate(PortNo);
    
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
            send_data_to_all_clients("AR");
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
            send_data_to_all_clients("BR");
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

/* 
 * reporting might be over kill ??
 */

void reportportstate(PIOPortSelect PortNo){

    if (PIOdebug) {
        printf("Port%c \n", PortNo==PIOPORTA ? 'A':'B');
        printf("\tPort Data   %2.2X\n",PIOPorts[PortNo].Portdata);
        printf("\tInt Vector  %4.4X\n",PIOPorts[PortNo].Portintvector);
        printf("\tInt Mask    %2.2X\n",PIOPorts[PortNo].Portintmask);  // control mode 3 - interrupt mask 0 means monitor
        printf("\tIO Mask     %2.2X\n",PIOPorts[PortNo].Portiomask);   // control mode 3 input (1) or output lines
        printf("\tLast int    %2.2X\n",PIOPorts[PortNo].Portlastintvalue); // holds the last interrupt value for control mode 3
        printf("\t2nd byte rd %2.2X\n",PIOPorts[PortNo].Portsecondbyteexpected); // set to 1 if expect a second byte
        printf("\tReady line  %2.2X\n",PIOPorts[PortNo].Portrdy);      // active high when data ready (output) or needed (input)
        // printf("\tStobe       %2.2X\n",PIOPorts[PortNo].Portstb);      // active low pulse to say data collected (output) or provided (input)
        printf("\tint allowed %2.2X\n",PIOPorts[PortNo].PortInterruptAllowed);
        printf("\tand or      %2.2X\n",PIOPorts[PortNo].Portandor);
        printf("\thigh low    %2.2X\n",PIOPorts[PortNo].Porthighlow);
        printf("\tInt Served  %2.2X\n",PIOPorts[PortNo].PortInterruptBeingServiced);    // set to 1 if interrupt was being serviced waiting for RETI
        printf("\tPort int    %2.2X\n",PIOPorts[PortNo].Portint);      // set 1 if int state exists 
        
    }
}



#define Pio_portA_display_x 1
#define Pio_portB_display_x 20
#define Pio_portA_display_y 12
#define Pio_portB_display_y 12


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
        status_display_set_char(connectortop,bitposx,posy,charcolour,STATUS_COLOR_BACKGROUND);
        status_display_set_char(connectorbottom,bitposx,posy+5,charcolour,STATUS_COLOR_BACKGROUND);
        bitposx++; // step on 1 character
        mask=mask>>1;
    }    
    // clear  INT info
    status_display_clear(' ',posx+10,posy+3,3,charcolour,STATUS_COLOR_BACKGROUND);
    status_display_clear(' ',posx+10,posy+5,3,charcolour,STATUS_COLOR_BACKGROUND);
    // clear  rdy info
    status_display_clear(' ',posx+10,posy+2,3,charcolour,STATUS_COLOR_BACKGROUND);
    status_display_clear(' ',posx+11,posy,1,charcolour,STATUS_COLOR_BACKGROUND);
    // clear masks lines
    status_display_clear(' ',posx+1,posy-2,18,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
    status_display_clear(' ',posx+1,posy-1,18,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
    // clear mode
    status_display_set_char(' ',posx+14,posy+2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
    status_display_set_char(' ',posx+14,posy+3,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);

    if (PioPortMode!=PIONONE) {
        // display mode
        status_display_set_char('M',posx+14,posy+2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
        status_display_set_char(PioPortMode+0x30,posx+14,posy+3,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
        //
        //status_display_set_char(connectortop,posx,posy,charcolour,STATUS_BACKGROUND);
        sprintf(stemp,"0X%2.2X",dataValue);
        // display the hex value of the data
        status_display_show_chars_full(stemp,posx+3,posy+2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
        // display the binay value of the data
        status_display_show_chars_full(binaryvalue,posx+1,posy+3,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
        if (PioPortMode==PIOCONTROL){
            // display mask bit
            // posx has not been incremented
            status_display_show_chars_full("IO Mask",posx+11,posy-1,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
            status_display_show_chars_full(binarymask,posx+1,posy-1,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
            if (PIOPorts[PortNo].PortInterruptAllowed==1){
                // show the interrupt stuff
                status_display_set_char(PIOPorts[PortNo].Portandor==0?'|':'&',posx+9,posy-2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
                status_display_set_char(PIOPorts[PortNo].Porthighlow==0?'v':'^',posx+10,posy-2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
                
                status_display_show_chars_full("Int Mask",posx+11,posy-2,STATUS_COLOR_BLACK,STATUS_COLOR_BACKGROUND);
                bitposx = posx + 1 ;  // these markers start in column 2
                // for each bit in the PIO port
                mask=0x80; // start with the top most bit
                for (binarybit=0;binarybit<8;binarybit++){
                    if (PIOPorts[PortNo].Portintmask & mask) {
                        // bit is 1
                        charcolour=STATUS_COLOR_BLACK;
                        binaryvalue[binarybit]='1';
                    } else {
                        charcolour=STATUS_COLOR_RED;
                        binaryvalue[binarybit]='0';
                    }

                    status_display_set_char(binaryvalue[binarybit],bitposx,posy-2,charcolour,STATUS_COLOR_BACKGROUND);
                    bitposx++; // step on 1 character
                    mask=mask>>1;
                }    
                
            }


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
            status_display_set_char(binaryvalue[0],posx+11,posy,charcolour,STATUS_COLOR_BACKGROUND);
        }
        // now set if we are processing an interrupt
        // Portint is set when an interrupt has been identified
        // PortInterruptBeingServiced is > 0 whilst interrupt processing is still going on
        if (PIOPorts[PortNo].PortInterruptAllowed==1){
            if (PIOPorts[PortNo].PortInterruptBeingServiced>0){
                // waiting - interrupt processing is ongoing
                // so technically IEO will be low blocking the lower priority stuff
                status_display_show_chars_full("IEO",posx+10,posy+5,STATUS_COLOR_BLUE,STATUS_COLOR_BACKGROUND);
            }
            if (PIOPorts[PortNo].Portint==1) {
                status_display_show_chars_full("INT",posx+10,posy+3,STATUS_COLOR_BLUE,STATUS_COLOR_BACKGROUND);
/*
                // bit is 1
                charcolour=STATUS_COLOR_RED;
                //binaryvalue[0]='1';
            } else {
                charcolour=STATUS_COLOR_BLUE;
                //binaryvalue[0]='0';
            }
            status_display_show_chars_full("Int",posx+10,posy+5,charcolour,STATUS_COLOR_BACKGROUND);
            // status_display_set_char(binaryvalue[0],posx+11,posy+5,charcolour,STATUS_COLOR_BACKGROUND);
*/
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
 * TODO - set up InterruptUnderProcess indicator
 *  0 means no interrupt in process
 *  1 means interrupt has been requested
 *  2 means interrupt acknowleged but waiting for RETI
 *  IEO will contilinue to be 0 until InterruptUnderProcess is 0
 * 
 * TODO - need to handle IEI and IEO correctly for multiples
 * and reset stuff when the RETI happens for this request
 * -- currently the ack cleasrs the int 
 * 
 * This routine will do any stuff the PIO needs
 * and check if interrupt is requested
 * 
 */
int PIOstatuscheck(int IEI, int *IEO) {
    
    int IEOlocal=IEI;

    if (IEOlocal > 0 ) {
        // nothing has more priority in the int chain
        // check if interrupt state has occurred 
        // Port A first and then Port B
        if (PIOPorts[PIOPORTA].PortInterruptBeingServiced>0){
            // we already requested an intterupt 
            // else we are waiting for the RETI
            IEOlocal=0;  // prevent lower devices from raising an interrupt
        } else {
            if (PIOPorts[PIOPORTA].Portint==1){ // we have an interrupt request state
                if (PIOdebug) {
                    printf("PIO PortA interrupt raised IEO set to 0\n");
                }
                IEOlocal=0;
                MaskableInterruptRequest++; // add 1 to the say we want to raise a maskable interrupt
                PIOPorts[PIOPORTA].PortInterruptBeingServiced=1;
            }
        }
    }
    // now check if PORTA has claimed priority
    if (IEOlocal > 0 ) {
        if (PIOPorts[PIOPORTB].PortInterruptBeingServiced>0){ 
                // we already requested an intterupt 
                // else we are waiting for the RETI
                IEOlocal=0;  // prevent lower devices from raising an interrupt
        } else {
            if (PIOPorts[PIOPORTB].Portint==1){
                if (PIOdebug) {
                    printf("PIO PortB interrupt raised IEO set to 0\n");
                }
                IEOlocal=0;
                MaskableInterruptRequest++; // add 1 to the say we want to raise a maskable interrupt
                PIOPorts[PIOPORTB].PortInterruptBeingServiced=1;
            }
        }
    }
    // TODO is this required ?
    // update status display 
    //displayPIOportAlines(PIOPORTA);
    //displayPIOportAlines(PIOPORTB);

    // tell what happened
    (*IEO)=IEOlocal;
    return IEOlocal;
   
}
/*
 * called when the main cycle in simz80 says it can handle a maskable interrupt
 * 
 * ir sets the VectorAddress if it is the device requested interrupt
 * 
 * if the request was made by this port this will reset the "int line"
 * but needs to keep the IEO line low until we do a RETI
 * 
 * 
 */

int PIOInterruptAcknowledge(int IEI, int *IEO){

    int IEOlocal=IEI;
    int returnvalue=0; // set to 1 if we have set vector 

    if (IEOlocal > 0 ) {

        if (PIOPorts[PIOPORTA].PortInterruptBeingServiced>1){
            printf("PIO Port A Interrupt Acknowledge bad PortInterruptBeingServiced %u\n",PIOPorts[PIOPORTA].PortInterruptBeingServiced);
            IEOlocal=0;
        }
        if (PIOPorts[PIOPORTA].PortInterruptBeingServiced==1){
            PIOPorts[PIOPORTA].Portint=0; // reset interrupt request
            if (PIOdebug) { 
                printf("PIO Port A - interrupt acknowledge vector %4.4X\n", PIOPorts[PIOPORTA].Portintvector);
                printf("Int line will be released, but awaiting RETI\n");
            }
            VectorAddress=PIOPorts[PIOPORTA].Portintvector;
            //displayPIOportAlines(PIOPORTA);
            MaskableInterruptRequest--; // say we have achknowlwdged that request 
            PIOPorts[PIOPORTA].PortInterruptBeingServiced=2; 
            IEOlocal=0;
            returnvalue=1;
        }
        
    }
    if (IEOlocal > 0 ) {
        if (PIOPorts[PIOPORTB].PortInterruptBeingServiced>1){
            printf("PIO Port B Interrupt Acknowledge bad PortInterruptBeingServiced %u\n",PIOPorts[PIOPORTB].PortInterruptBeingServiced);
            IEOlocal=0;
        }
        if (PIOPorts[PIOPORTB].PortInterruptBeingServiced==1){
            PIOPorts[PIOPORTB].Portint=0;  // reset interrupt request
            if (PIOdebug) { 
                printf("PIO Port B - interrupt acknowledge vector %4.4X\n", PIOPorts[PIOPORTA].Portintvector);
                printf("Int line will be released, but awaiting RETI\n");
            }
            VectorAddress=PIOPorts[PIOPORTB].Portintvector;
            //displayPIOportAlines(PIOPORTB);
            MaskableInterruptRequest--; // say we have achknowlwdged that request 
            PIOPorts[PIOPORTB].PortInterruptBeingServiced=2; 
            IEOlocal=0;
            returnvalue=1;
        }
    }
    // update status display 
    displayPIOportAlines(PIOPORTA);
    displayPIOportAlines(PIOPORTB);


    // tell what happened
    (*IEO)=IEOlocal;
    return returnvalue;
    
}


/*
 * called when the main cycle in simz80 sees an RETI command
 * 
 * This will set the ports PortInterruptBeingServiced tp 0 
 * if the IEI is high
 * 
 * This will result in the IEO being set to 1 when the next status routine is called
 * 
 */

void PIOCheckRETI(int IEI, int *IEO){

    int IEOlocal=IEI;

    if (IEOlocal > 0 ) {

        if (PIOPorts[PIOPORTA].PortInterruptBeingServiced>1){
            PIOPorts[PIOPORTA].PortInterruptBeingServiced=0;
            IEOlocal=0; // we are still in the process of completing the interrup service
            if (PIOdebug) { 
                printf("PIO PORTA - RETI identified  - IEO will become 1\n");
            }
            
        }
        
    }
    if (IEOlocal > 0 ) {
        if (PIOPorts[PIOPORTB].PortInterruptBeingServiced>1){
            PIOPorts[PIOPORTB].PortInterruptBeingServiced=0;
            IEOlocal=0; // we are still in the process of completing the interrup service
            if (PIOdebug) { 
                printf("PIO PORTB - RETI identified - IEO will become 1\n");
            }
        }
    }
    // update status display 
    displayPIOportAlines(PIOPORTA);
    displayPIOportAlines(PIOPORTB);

    // tell what happened
    (*IEO)=IEOlocal;
    
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


    if (PIOdebug) { 
        printf("Device read Port  %c value %2.2X \n", PortNo==PIOPORTA ? 'A':'B',PIOPorts[PortNo].Portdata);
    }
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
            PIOPorts[PortNo].Portint=1;  // activate an interrupt
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
            PIOPorts[PortNo].Portint=1;  // activate an interrupt request 
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
    if (PIOdebug) { 
        printf("Device write Port  %c value %2.2X port now %2.2X\n", PortNo==PIOPORTA ? 'A':'B',value, PIOPorts[PortNo].Portdata);
    }
    displayPIOportAlines(PortNo);

    return value;
    
}

/*
 * read the state of the ports ready line
 */
unsigned char PIOReadReadyLine(PIOPortSelect PortNo){

    if (PIOdebug) { 

        printf("Device read Port %c ready line %2.2X \n", PortNo==PIOPORTA ? 'A':'B',PIOPorts[PortNo].Portrdy);
    }
 
 
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
                PIOPorts[PortNo].Portint=1;  // activate an interrupt
            }
            break;
        case PIOOUT:
            PIOPorts[PortNo].Portrdy=0;
            if (PIOPorts[PortNo].PortInterruptAllowed){
                PIOPorts[PortNo].Portint=1;  // activate an interrupt
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
        
    if (PIOdebug) { 
        printf("Device strobe Port%c - port int %d\n", PortNo==PIOPORTA ? 'A':'B', PIOPorts[PortNo].Portint);
    }
    return 0;
}

// end of code