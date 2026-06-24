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


unsigned char Pio_portAdata=0;
unsigned char Pio_portBdata=0;
unsigned char Pio_portAcontrol=0;
unsigned char Pio_portBcontrol=0;
unsigned char Pio_portAintvector=0;
unsigned char Pio_portBintvector=0;

unsigned char Pio_portAandor=0;
unsigned char Pio_portBandor=0;

unsigned char Pio_portAhighlow=0;
unsigned char Pio_portBhighlow=0;

unsigned char Pio_portA_intmask=0;
unsigned char Pio_portB_intmask=0;

unsigned char Pio_portA_iomask=0;
unsigned char Pio_portB_iomask=0;


int Pio_portAsecondbyteexpected=0; // set to 1 if expect a second byte
int Pio_portBsecondbyteexpected=0; // set to 1 if expect a second byte

PIOPortMode Pio_portAmode=PIONONE;
PIOPortMode Pio_portBmode=PIONONE;

/* called to reset the PIO state 
 * as per the datasheet
 */
 
void PIOreset(){
 
    Pio_portAmode=PIONONE;
    Pio_portBmode=PIONONE;
   
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
    
    if ( (value & 0x0F) == 0x0F ) {
        
        if (PIOPort==PIOPORTA){
            // set port mode 
            Pio_portAmode = mode1;
        } else {
            if (mode1 == 2){
                // not allowed for Port B
                fprintf(stderr,"Port B cannot be bidirectional ignored");
            }else {
                // set port mode 
                Pio_portBmode = mode1;
            }
        }
        if (mode1==3){ // only if in mode 3 expect the io mask
                        // set to 1 means input set to 0 means output
                        // During Mode 3 operation, the strode signal is ignored and the Ready line is held Low. 
            retval=1;
        }
    } else if ((value & 0x0F)==0x07 ){  
        // interupt vector setting
        // bit 7 enables interupt ( 1 to enable )
        // bit 6 selects either and (1) or or (0)
        // bit 5 selects high (1) or low (0)
        // bit 4 set if int mask follows 
        if (value & 0x80) { // bit d7
            if (mode1==3) { // but only if mode 3 control mode
                retval=2; // say mask to follow
            }
        }


    }
    return retval;
}


void PIO_Porta_data_out(unsigned char value){
    if (Pio_portAmode==PIOOUT){
        Pio_portAdata=value;
        displayPIOportAlines(PIOPORTA);
    }
}
void PIO_Portb_data_out(unsigned char value){
    if (Pio_portBmode==PIOOUT){
        Pio_portBdata=value;
        displayPIOportAlines(PIOPORTB);
    }
}
void PIO_Porta_control_out(unsigned char value){
    int retval=0;
    if (Pio_portAsecondbyteexpected){
        if (Pio_portAsecondbyteexpected==1){
            // save IO mask for mode 3 control
            Pio_portA_iomask=value;
        } else {
            // save then interupt mask
            Pio_portA_intmask=value;
        }
        Pio_portAsecondbyteexpected=0; // reset second byte expected
    } else {
        Pio_portAcontrol=value;
        if (value & 0x01){
            // set the mode and see if a second byte is to be expected
            retval=PIO_Set_Mode(PIOPORTA,value);
            if (retval==0){
                Pio_portAsecondbyteexpected=0;
            }else {
                Pio_portAsecondbyteexpected=retval;
            }
        }
    }
    Pio_portAcontrol=value;
    displayPIOportAlines(PIOPORTA);
}

void PIO_Portb_control_out(unsigned char value){
    int retval=0;
    if (Pio_portBsecondbyteexpected){
        if (Pio_portBsecondbyteexpected==1){
            // save IO mask for mode 3 control
            Pio_portB_iomask=value;
        } else {
            // save then interupt mask
            Pio_portB_intmask=value;
        }
        Pio_portBsecondbyteexpected=0; // reset second byte expected
    } else {
        Pio_portBcontrol=value;
        if (value & 0x01){
            // set the mode and see if a second byte is to be expected
            retval=PIO_Set_Mode(PIOPORTB,value);
            if (retval==0){
                Pio_portBsecondbyteexpected=0;
            }else {
                Pio_portBsecondbyteexpected=retval;
            }
        }
    }
    Pio_portBcontrol=value;
    displayPIOportAlines(PIOPORTB);
    
}


int PIO_Porta_data_in(){
        return Pio_portAdata;
}

int PIO_Portb_data_in(){
        return Pio_portBdata;
   
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

    status_display_show_chars("CPU",15,10);
    

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
    line1[9]=0x91;
    line1[10]=0x0;
    status_display_show_chars(line1,posx,posy+1);
    // vertical line at each end and spaces in middle
    line1[0]=0x94; //
    for (int pos1=1;pos1<9;pos1++){
        line1[pos1]=0x20;
    }
    line1[9]=0x94;
    line1[10]=0x0;
    status_display_show_chars(line1,posx,posy+2);
    status_display_show_chars(line1,posx,posy+3);
    // bottom line
    line1[0]=0x92;
    for (int pos1=1;pos1<9;pos1++){
        line1[pos1]=0x9A;
    }
    line1[9]=0x93;
    line1[10]=0x0;
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

void displayPIOportAlines(PIOPortSelect PIOPort){
    
    PIOPortMode PioPortMode = Pio_portBmode;
    int posx=Pio_portB_display_x;
    int posy=Pio_portB_display_y;
    unsigned char dataValue=Pio_portBdata;
    unsigned char iovalue=Pio_portB_iomask;
    char  stemp[10];      // temp.String für sprintf()
    char  binaryvalue[9]; // set to the binary value 8 plus 0
    char  binarymask[9]; // set to the binary value 8 plus 0
    int   binarybit=0;  // used to set the binary bit in above
    unsigned int mask=0x01;
    binaryvalue[8]=0; // set end marker
    binarymask[8]=0; // set to the binary value 8 plus 0

    if(PIOPort==PIOPORTA){
        PioPortMode = Pio_portAmode;
        posx=Pio_portA_display_x;
        posy=Pio_portA_display_y;
        dataValue=Pio_portAdata;
        iovalue=Pio_portA_iomask;
    }

// display the top lines which connect to the CPU
    unsigned char connectortop = 0xB2; //  arrow down 
    unsigned char connectorbottom = 0xB2; //  arrow down 
    uint32_t charcolour;
    posx++ ;  // these markers start in column 2
    // for each bit in the PIO port
    mask=0x80; // start with the top most bit
    for (binarybit=0;binarybit<8;binarybit++){
        if (dataValue & mask) {
            // bit is 1
            charcolour=STATUS_RED;
            binaryvalue[binarybit]='1';
        } else {
            charcolour=STATUS_BLUE;
            binaryvalue[binarybit]='0';
        }

        switch (PioPortMode) {
            case PIOIN:
                connectortop = 0xB3; //  arrow up
                connectorbottom = 0xB3; //  arrow up 
                break;
            case PIOOUT:
                connectortop = 0xB2; //  arrow down
                connectorbottom = 0xB2; //  arrow down 
                break;
            case PIOBIDIRECTIONAL:
                connectortop = 0xB2; //  arrow down
                connectorbottom = 0xB2; //  arrow down 
                break;
            case PIOCONTROL:
                if (iovalue & mask ){
                    connectortop = 0xB3; //  arrow up
                    connectorbottom = 0xB3; //  arrow up 
                    binarymask[binarybit]='1';
                } else {
                    connectortop = 0xB2; //  arrow down
                    connectorbottom = 0xB2; //  arrow down 
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
        status_display_show_char_full(connectortop,posx,posy,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,charcolour,STATUS_BACKGROUND);
        status_display_show_char_full(connectorbottom,posx,posy+5,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,charcolour,STATUS_BACKGROUND);
        posx++; // step on 1 character
        mask=mask>>1;
    }    
    if (PioPortMode!=PIONONE) {
        //status_display_show_char_full(connectortop,posx,posy,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,charcolour,STATUS_BACKGROUND);
        sprintf(stemp,"0X%2.2X",dataValue);
        // posx has been incremented 
        status_display_show_chars_full(stemp,posx-6,posy+2,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_BLACK,STATUS_BACKGROUND);
        status_display_show_chars_full(binaryvalue,posx-8,posy+3,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_BLACK,STATUS_BACKGROUND);
        if (PioPortMode==PIOCONTROL){
            // display mask bit
            status_display_show_chars_full("IO Mask",posx+1,posy+2,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_BLACK,STATUS_BACKGROUND);
            status_display_show_chars_full(binarymask,posx+1,posy+3,STATUS_DISPLAYSCALEX,STATUS_DISPLAYSCALEY,STATUS_BLACK,STATUS_BACKGROUND);
        }
    }

}
    
    



