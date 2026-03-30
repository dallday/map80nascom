/***
 *  Z80 Disassembler
 *
 ***/

#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "simz80.h"
#include "cpmswitch.h"
#include "disassemble.h"

#define DEBUGGER        0               // if 1, you end up with calculated


static int OpcodeLen(unsigned char *programdata);
static void  Disassemble(unsigned int adr, unsigned char * opcodedata, char * returnstr);

// Determine the length of an opcode in bytes
// parameter is string (char * )
// returns length in bytes

static int OpcodeLen( unsigned char *programdata)
{
int len = 1;

    switch((unsigned char)programdata[0]) {// Opcode
    case 0x06:          // LD B,n
    case 0x0E:          // LD C,n
    case 0x10:          // DJNZ e
    case 0x16:          // LD D,n
    case 0x18:          // JR e
    case 0x1E:          // LD E,n
    case 0x20:          // JR NZ,e
    case 0x26:          // LD H,n
    case 0x28:          // JR Z,e
    case 0x2E:          // LD L,n
    case 0x30:          // JR NC,e
    case 0x36:          // LD (HL),n
    case 0x38:          // JR C,e
    case 0x3E:          // LD A,n
    case 0xC6:          // ADD A,n
    case 0xCE:          // ADC A,n
    case 0xD3:          // OUT (n),A
    case 0xD6:          // SUB n
    case 0xDB:          // IN A,(n)
    case 0xDE:          // SBC A,n
    case 0xE6:          // AND n
    case 0xEE:          // XOR n
    case 0xF6:          // OR n
    case 0xFE:          // CP n

    case 0xCB:          // Shift-,Rotate-,Bit-
                len = 2;
                break;
    case 0x01:          // LD BC,nn'
    case 0x11:          // LD DE,nn'
    case 0x21:          // LD HL,nn'
    case 0x22:          // LD (nn'),HL
    case 0x2A:          // LD HL,(nn')
    case 0x31:          // LD SP,(nn')
    case 0x32:          // LD (nn'),A
    case 0x3A:          // LD A,(nn')
    case 0xC2:          // JP NZ,nn'
    case 0xC3:          // JP nn'
    case 0xC4:          // CALL NZ,nn'
    case 0xCA:          // JP Z,nn'
    case 0xCC:          // CALL Z,nn'
    case 0xCD:          // CALL nn'
    case 0xD2:          // JP NC,nn'
    case 0xD4:          // CALL NC,nn'
    case 0xDA:          // JP C,nn'
    case 0xDC:          // CALL C,nn'
    case 0xE2:          // JP PO,nn'
    case 0xE4:          // CALL PO,nn'
    case 0xEA:          // JP PE,nn'
    case 0xEC:          // CALL PE,nn'
    case 0xF2:          // JP P,nn'
    case 0xF4:          // CALL P,nn'
    case 0xFA:          // JP M,nn'
    case 0xFC:          // CALL M,nn'
                len = 3;
                break;
    case 0xDD:  len = 2;
                switch((unsigned char)programdata[1]) {// 2.Part of the opcode
                case 0x34:          // INC (IX+d)
                case 0x35:          // DEC (IX+d)
                case 0x46:          // LD B,(IX+d)
                case 0x4E:          // LD C,(IX+d)
                case 0x56:          // LD D,(IX+d)
                case 0x5E:          // LD E,(IX+d)
                case 0x66:          // LD H,(IX+d)
                case 0x6E:          // LD L,(IX+d)
                case 0x70:          // LD (IX+d),B
                case 0x71:          // LD (IX+d),C
                case 0x72:          // LD (IX+d),D
                case 0x73:          // LD (IX+d),E
                case 0x74:          // LD (IX+d),H
                case 0x75:          // LD (IX+d),L
                case 0x77:          // LD (IX+d),A
                case 0x7E:          // LD A,(IX+d)
                case 0x86:          // ADD A,(IX+d)
                case 0x8E:          // ADC A,(IX+d)
                case 0x96:          // SUB A,(IX+d)
                case 0x9E:          // SBC A,(IX+d)
                case 0xA6:          // AND (IX+d)
                case 0xAE:          // XOR (IX+d)
                case 0xB6:          // OR (IX+d)
                case 0xBE:          // CP (IX+d)
                            len = 3;
                            break;
                case 0x21:          // LD IX,nn'
                case 0x22:          // LD (nn'),IX
                case 0x2A:          // LD IX,(nn')
                case 0x36:          // LD (IX+d),n
                case 0xCB:          // Rotation (IX+d)
                            len = 4;
                            break;
                }
                break;
    case 0xED:  len = 2;
                switch(programdata[1]) {// 2.Part of the opcode
                case 0x43:          // LD (nn'),BC
                case 0x4B:          // LD BC,(nn')
                case 0x53:          // LD (nn'),DE
                case 0x5B:          // LD DE,(nn')
                case 0x73:          // LD (nn'),SP
                case 0x7B:          // LD SP,(nn')
                            len = 4;
                            break;
                }
                break;
    case 0xFD:  len = 2;
                switch((unsigned char)programdata[1]) {// 2.Part of the opcode
                case 0x34:          // INC (IY+d)
                case 0x35:          // DEC (IY+d)
                case 0x46:          // LD B,(IY+d)
                case 0x4E:          // LD C,(IY+d)
                case 0x56:          // LD D,(IY+d)
                case 0x5E:          // LD E,(IY+d)
                case 0x66:          // LD H,(IY+d)
                case 0x6E:          // LD L,(IY+d)
                case 0x70:          // LD (IY+d),B
                case 0x71:          // LD (IY+d),C
                case 0x72:          // LD (IY+d),D
                case 0x73:          // LD (IY+d),E
                case 0x74:          // LD (IY+d),H
                case 0x75:          // LD (IY+d),L
                case 0x77:          // LD (IY+d),A
                case 0x7E:          // LD A,(IY+d)
                case 0x86:          // ADD A,(IY+d)
                case 0x8E:          // ADC A,(IY+d)
                case 0x96:          // SUB A,(IY+d)
                case 0x9E:          // SBC A,(IY+d)
                case 0xA6:          // AND (IY+d)
                case 0xAE:          // XOR (IY+d)
                case 0xB6:          // OR (IY+d)
                case 0xBE:          // CP (IY+d)
                            len = 3;
                            break;
                case 0x21:          // LD IY,nn'
                case 0x22:          // LD (nn'),IY
                case 0x2A:          // LD IY,(nn')
                case 0x36:          // LD (IY+d),n
                case 0xCB:          // Rotation,Bitop (IY+d)
                            len = 4;
                            break;
                }
                break;
    //case 0xDF:      // RST 18 - special case for nassys code 
    //            len=2; // the extra bytes will be displayed else where
    //            break;
    }
    return(len);
}


// Disassemble - return string with instruction and data
// maximum number of bytes is 4 for longest opcode

static void  Disassemble(unsigned int adr, unsigned char * opcodedata, char * returnstr)
{

// get first opcode
unsigned char  firstOpcode = opcodedata[0];
unsigned char  d = (firstOpcode >> 3) & 7;
unsigned char  e = firstOpcode & 7;
int            pointer1=0; // where the opcode is

static char *  reg[8] = {"B","C","D","E","H","L","(HL)","A"};
static char *  dreg[4] = {"BC","DE","HL","SP"};
static char *  cond[8] = {"NZ","Z","NC","C","PO","PE","P","M"};
static char *  arith[8] = {"ADD  A,","ADC  A,","SUB  ","SBC  A,","AND  ","XOR  ","OR  ","CP  "};

char           stemp[80];      // temp.String für sprintf()
char           ireg[3];        // temp.Indexregister

    switch(firstOpcode & 0xC0) {
    case 0x00:
        switch(e) {
        case 0x00:
            switch(d) {
            case 0x00:
                strcpy(returnstr,"NOP");
                break;
            case 0x01:
                strcpy(returnstr,"EX  AF,AF'");
                break;
            case 0x02:
                strcpy(returnstr,"DJNZ ");
                sprintf(stemp,"%4.4Xh",adr+2+(int8_t)opcodedata[pointer1+1]);
                strcat(returnstr,stemp);
                break;
            case 0x03:
                strcpy(returnstr,"JR  ");
                sprintf(stemp,"%4.4Xh",adr+2+(int8_t)opcodedata[pointer1+1]);
                strcat(returnstr,stemp);
                break;
            default:
                strcpy(returnstr,"JR  ");
                strcat(returnstr,cond[d & 3]);
                strcat(returnstr,",");
                sprintf(stemp,"%4.4Xh",adr+2+(int8_t)opcodedata[pointer1+1]);strcat(returnstr,stemp);
                break;
            }
            break;
        case 0x01:
            if(firstOpcode & 0x08) {
                strcpy(returnstr,"ADD  HL,");
                strcat(returnstr,dreg[d >> 1]);
            } else {
                strcpy(returnstr,"LD  ");
                strcat(returnstr,dreg[d >> 1]);
                strcat(returnstr,",");
                sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
            }
            break;
        case 0x02:
            switch(d) {
            case 0x00:
                strcpy(returnstr,"LD  (BC),A");
                break;
            case 0x01:
                strcpy(returnstr,"LD A,(BC)");
                break;
            case 0x02:
                strcpy(returnstr,"LD  (DE),A");
                break;
            case 0x03:
                strcpy(returnstr,"LD  A,(DE)");
                break;
            case 0x04:
                strcpy(returnstr,"LD  (");
                sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                strcat(returnstr,"),HL");
                break;
            case 0x05:
                strcpy(returnstr,"LD  HL,(");
                sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                strcat(returnstr,")");
                break;
            case 0x06:
                strcpy(returnstr,"LD  (");
                sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                strcat(returnstr,"),A");
                break;
            case 0x07:
                strcpy(returnstr,"LD  A,(");
                sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                strcat(returnstr,")");
                break;
            }
            break;
        case 0x03:
            if(firstOpcode & 0x08)
                strcpy(returnstr,"DEC  ");
            else
                strcpy(returnstr,"INC  ");
            strcat(returnstr,dreg[d >> 1]);
            break;
        case 0x04:
            strcpy(returnstr,"INC  ");
            strcat(returnstr,reg[d]);
            break;
        case 0x05:
            strcpy(returnstr,"DEC  ");
            strcat(returnstr,reg[d]);
            break;
        case 0x06:              // LD   d,n
            strcpy(returnstr,"LD  ");
            strcat(returnstr,reg[d]);
            strcat(returnstr,",");
            sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
            break;
        case 0x07:
            {
            static char * str[8] = {"RLCA","RRCA","RLA","RRA","DAA","CPL","SCF","CCF"};
            strcpy(returnstr,str[d]);
            }
            break;
        }
        break;
    case 0x40:                          // LD   d,s
        if(d == e) {
            strcpy(returnstr,"HALT");
        } else {
            strcpy(returnstr,"LD  ");
            strcat(returnstr,reg[d]);
            strcat(returnstr,",");
            strcat(returnstr,reg[e]);
        }
        break;
    case 0x80:
        strcpy(returnstr,arith[d]);
        strcat(returnstr,reg[e]);
        break;
    case 0xC0:
        switch(e) {
        case 0x00:
            strcpy(returnstr,"RET  ");
            strcat(returnstr,cond[d]);
            break;
        case 0x01:
            if(d & 1) {
                switch(d >> 1) {
                case 0x00:
                    strcpy(returnstr,"RET");
                    break;
                case 0x01:
                    strcpy(returnstr,"EXX");
                    break;
                case 0x02:
                    strcpy(returnstr,"JP  (HL)");
                    break;
                case 0x03:
                    strcpy(returnstr,"LD  SP,HL");
                    break;
                }
            } else {
                strcpy(returnstr,"POP  ");
                if((d >> 1)==3)
                    strcat(returnstr,"AF");
                else
                    strcat(returnstr,dreg[d >> 1]);
            }
            break;
        case 0x02:
            strcpy(returnstr,"JP  ");
            strcat(returnstr,cond[d]);
            strcat(returnstr,",");
            sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
            break;
        case 0x03:
            switch(d) {
            case 0x00:
                strcpy(returnstr,"JP  ");
                sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                break;
            case 0x01:                  // 0xCB
                // pointer1=1;
                firstOpcode = opcodedata[++pointer1];     // Get extension opcode
                //++adr;
                d = (firstOpcode >> 3) & 7;
                e = firstOpcode & 7;
                stemp[1] = 0;           // temp.String = 1 character
                switch(firstOpcode & 0xC0) {
                case 0x00:
                    {
                    static char * str[8] = {"RLC","RRC","RL","RR","SLA","SRA","???","SRL"};
                    strcpy(returnstr,str[d]);
                    }
                    strcat(returnstr,"  ");
                    strcat(returnstr,reg[e]);
                    break;
                case 0x40:
                    strcpy(returnstr,"BIT  ");
                    stemp[0] = d+'0';strcat(returnstr,stemp);
                    strcat(returnstr,",");
                    strcat(returnstr,reg[e]);
                    break;
                case 0x80:
                    strcpy(returnstr,"RES  ");
                    stemp[0] = d+'0';strcat(returnstr,stemp);
                    strcat(returnstr,",");
                    strcat(returnstr,reg[e]);
                    break;
                case 0xC0:
                    strcpy(returnstr,"SET  ");
                    stemp[0] = d+'0';strcat(returnstr,stemp);
                    strcat(returnstr,",");
                    strcat(returnstr,reg[e]);
                    break;
                }
                break;
            case 0x02:
                strcpy(returnstr,"OUT  (");
                sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                strcat(returnstr,"),A");
                break;
            case 0x03:
                strcpy(returnstr,"IN  A,(");
                sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                strcat(returnstr,")");
                break;
            case 0x04:
                strcpy(returnstr,"EX  (SP),HL");
                break;
            case 0x05:
                strcpy(returnstr,"EX  DE,HL");
                break;
            case 0x06:
                strcpy(returnstr,"DI");
                break;
            case 0x07:
                strcpy(returnstr,"EI");
                break;
            }
            break;
        case 0x04:
            strcpy(returnstr,"CALL ");
            strcat(returnstr,cond[d]);
            strcat(returnstr,",");
            sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
            break;
        case 0x05:
            if(d & 1) {
                switch(d >> 1) {
                case 0x00:
                    strcpy(returnstr,"CALL ");
                    sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                    break;
                case 0x02:              // 0xED

                    firstOpcode = opcodedata[++pointer1]; // Get extension opcode
                    d = (firstOpcode >> 3) & 7;
                    e = firstOpcode & 7;
                    switch(firstOpcode & 0xC0) {
                    case 0x40:
                        switch(e) {
                        case 0x00:
                            strcpy(returnstr,"IN  ");
                            strcat(returnstr,reg[d]);
                            strcat(returnstr,",(C)");
                            break;
                        case 0x01:
                            strcpy(returnstr,"OUT  (C),");
                            strcat(returnstr,reg[d]);
                            break;
                        case 0x02:
                            if(d & 1)
                                strcpy(returnstr,"ADC");
                            else
                                strcpy(returnstr,"SBC");
                            strcat(returnstr,"  HL,");
                            strcat(returnstr,dreg[d >> 1]);
                            break;
                        case 0x03:
                            if(d & 1) {
                                strcpy(returnstr,"LD  ");
                                strcat(returnstr,dreg[d >> 1]);
                                strcat(returnstr,",(");
                                sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                                strcat(returnstr,")");
                            } else {
                                strcpy(returnstr,"LD  (");
                                sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                                strcat(returnstr,"),");
                                strcat(returnstr,dreg[d >> 1]);
                            }
                            break;
                        case 0x04:
                            {
                            static char * str[8] = {"NEG","???","???","???","???","???","???","???"};
                            strcpy(returnstr,str[d]);
                            }
                            break;
                        case 0x05:
                            {
                            static char * str[8] = {"RETN","RETI","???","???","???","???","???","???"};
                            strcpy(returnstr,str[d]);
                            }
                            break;
                        case 0x06:
                            strcpy(returnstr,"IM  ");
                            stemp[0] = d + '0' - 1; stemp[1] = 0;
                            strcat(returnstr,stemp);
                            break;
                        case 0x07:
                            {
                            static char * str[8] = {"LD  I,A","???","LD  A,I","???","RRD","RLD","???","???"};
                            strcpy(returnstr,str[d]);
                            }
                            break;
                        }
                        break;
                    case 0x80:
                        {
                        static char * str[32] = {"LDI","CPI","INI","OUTI","???","???","???","???",
                                              "LDD","CPD","IND","OUTD","???","???","???","???",
                                              "LDIR","CPIR","INIR","OTIR","???","???","???","???",
                                              "LDDR","CPDR","INDR","OTDR","???","???","???","???"};
                        strcpy(returnstr,str[firstOpcode & 0x1F]);
                        }
                        break;
                    }
                    break;
                default:                // 0x01 (0xDD) = IX, 0x03 (0xFD) = IY
                    strcpy(ireg,(firstOpcode & 0x20)?"IY":"IX");
                    firstOpcode = opcodedata[++pointer1]; // Erweiterungsopcode holen
                    switch(firstOpcode) {
                    case 0x09:
                        strcpy(returnstr,"ADD  ");
                        strcat(returnstr,ireg);
                        strcat(returnstr,",BC");
                        break;
                    case 0x19:
                        strcpy(returnstr,"ADD  ");
                        strcat(returnstr,ireg);
                        strcat(returnstr,",DE");
                        break;
                    case 0x21:
                        strcpy(returnstr,"LD  ");
                        strcat(returnstr,ireg);
                        strcat(returnstr,",");
                        sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                        break;
                    case 0x22:
                        strcpy(returnstr,"LD  (");
                        sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                        strcat(returnstr,"),");
                        strcat(returnstr,ireg);
                        break;
                    case 0x23:
                        strcpy(returnstr,"INC  ");
                        strcat(returnstr,ireg);
                        break;
                    case 0x29:
                        strcpy(returnstr,"ADD  ");
                        strcat(returnstr,ireg);
                        strcat(returnstr,",");
                        strcat(returnstr,ireg);
                        break;
                    case 0x2A:
                        strcpy(returnstr,"LD  ");
                        strcat(returnstr,ireg);
                        strcat(returnstr,",(");
                        sprintf(stemp,"%4.4Xh",opcodedata[pointer1+1]+(opcodedata[pointer1+2]<<8));strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0x2B:
                        strcpy(returnstr,"DEC  ");
                        strcat(returnstr,ireg);
                        break;
                    case 0x34:
                        strcpy(returnstr,"INC  (");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0x35:
                        strcpy(returnstr,"DEC  (");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0x36:
                        strcpy(returnstr,"LD  (");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,"),");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+2]);strcat(returnstr,stemp);
                        break;
                    case 0x39:
                        strcpy(returnstr,"ADD  ");
                        strcat(returnstr,ireg);
                        strcat(returnstr,",SP");
                        break;
                    case 0x46:
                    case 0x4E:
                    case 0x56:
                    case 0x5E:
                    case 0x66:
                    case 0x6E:
                        strcpy(returnstr,"LD  ");
                        strcat(returnstr,reg[(firstOpcode>>3)&7]);
                        strcat(returnstr,",(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0x70:
                    case 0x71:
                    case 0x72:
                    case 0x73:
                    case 0x74:
                    case 0x75:
                    case 0x77:
                        strcpy(returnstr,"LD  (");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,"),");
                        strcat(returnstr,reg[firstOpcode & 7]);
                        break;
                    case 0x7E:
                        strcpy(returnstr,"LD  A,(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0x86:
                        strcpy(returnstr,"ADD  A,(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0x8E:
                        strcpy(returnstr,"ADC  A,(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0x96:
                        strcpy(returnstr,"SUB  (");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0x9E:
                        strcpy(returnstr,"SBC  A,(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0xA6:
                        strcpy(returnstr,"AND  A,(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0xAE:
                        strcpy(returnstr,"XOR  A,(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0xB6:
                        strcpy(returnstr,"OR  A,(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0xBE:
                        strcpy(returnstr,"CP  A,(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    case 0xE1:
                        strcpy(returnstr,"POP  ");
                        strcat(returnstr,ireg);
                        break;
                    case 0xE3:
                        strcpy(returnstr,"EX  (SP),");
                        strcat(returnstr,ireg);
                        break;
                    case 0xE5:
                        strcpy(returnstr,"PUSH ");
                        strcat(returnstr,ireg);
                        break;
                    case 0xE9:
                        strcpy(returnstr,"JP  (");
                        strcat(returnstr,ireg);
                        strcat(returnstr,")");
                        break;
                    case 0xF9:
                        strcpy(returnstr,"LD  SP,");
                        strcat(returnstr,ireg);
                        break;
                    case 0xCB:
                        firstOpcode = opcodedata[pointer1+2]; // weiteren Unteropcode
                        d = (firstOpcode >> 3) & 7;
                        stemp[1] = 0;
                        switch(firstOpcode & 0xC0) {
                        case 0x00:
                            {
                            static char * str[8] = {"RLC","RRC","RL","RR","SLA","SRA","???","SRL"};
                            strcpy(returnstr,str[d]);
                            }
                            strcat(returnstr,"  ");
                            break;
                        case 0x40:
                            strcpy(returnstr,"BIT  ");
                            stemp[0] = d + '0';
                            strcat(returnstr,stemp);
                            strcat(returnstr,",");
                            break;
                        case 0x80:
                            strcpy(returnstr,"RES  ");
                            stemp[0] = d + '0';
                            strcat(returnstr,stemp);
                            strcat(returnstr,",");
                            break;
                        case 0xC0:
                            strcpy(returnstr,"SET  ");
                            stemp[0] = d + '0';
                            strcat(returnstr,stemp);
                            strcat(returnstr,",");
                            break;
                        }
                        strcat(returnstr,"(");
                        strcat(returnstr,ireg);
                        strcat(returnstr,"+");
                        sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);strcat(returnstr,stemp);
                        strcat(returnstr,")");
                        break;
                    }
                    break;
                }
            } else {
                strcpy(returnstr,"PUSH ");
                if((d >> 1)==3)
                    strcat(returnstr,"AF");
                else
                    strcat(returnstr,dreg[d >> 1]);
            }
            break;
        case 0x06:
            strcpy(returnstr,arith[d]);
            sprintf(stemp,"%2.2Xh",opcodedata[pointer1+1]);
            strcat(returnstr,stemp);
            break;
        case 0x07:
            // all the reset codes 
            strcpy(returnstr,"RST  ");
            sprintf(stemp,"%2.2Xh",firstOpcode & 0x38);
            strcat(returnstr,stemp);
            break;
        }
        break;
    }
}

// process a set of opcodes
// not brilliant bit of code but needed to go between the emulator ram and the disassembler string 
// 

int disassembleline (unsigned int address, unsigned char * bindata, char * returnedline)
{

    char   stemp[100];

    uint16_t    len;
    uint16_t    i;

        //printf("Processing line\n");


        len = OpcodeLen(&bindata[0]);           // get length of opcode in bytes

        sprintf(returnedline,"%4.4X: ",address);

        // output the bytes
        for(i=0;i<len;i++){
            sprintf(stemp,"%2.2X ",(unsigned char)bindata[i]);
            strcat(returnedline,stemp);
        }
        // output spaces for unused byte spaces
        for(i=4;i>len;i--){
            strcat(returnedline,"   ");
        }
        // final separater
        strcat(returnedline," ");

        Disassemble(address,&bindata[0],stemp);
        while (strlen(stemp)<16){
            strcat(stemp," ");
        }
        strcat(returnedline,stemp);

        return len;
        //printf("%s\n",returnedline);

}

int disassembleprogram(FASTREG PC,
                        FILE * outputfile,
                        int showregisters,
                        FASTREG startaddress,
                        FASTREG endaddress,
                        FASTREG AF,
                        FASTREG BC,
                        FASTREG DE,
                        FASTREG HL,
                        FASTREG SP){

        static FASTREG previousPC=0;
        // added limit range to the trace option
        //printf("Tracing only between address %4.4X and %4.4X (hex) \n",tracestartaddress,traceendaddress);
        
        char disstr[100];
        unsigned char disdata[5];
        disdata[0] = RAM(PC);
        disdata[1] = RAM(PC+1);
        disdata[2] = RAM(PC+2);
        disdata[3] = RAM(PC+3);
        disdata[4] = 0x00;
        int lencmd = 0 ;
        // not using the returned len of the command but is needed on other calls
        // now sending it on to the calling code
        lencmd=disassembleline(PC,disdata,disstr);
        // to stop the warning message
        if (0) fprintf(outputfile,"command len %d\n",lencmd);

        // printf("tracing PC %4.4X %4.4X %4.4X\n",PC,startaddress,endaddress);

        if ( (PC>=startaddress) && (PC <= endaddress) )  {

            fprintf(outputfile,"::%s",disstr);
            if (showregisters){
                fprintf(outputfile, " SP=%4.4X AF=%4.4X HL=%4.4X DE=%4.4X BC=%4.4X", 
                                SP & 0xFFFF, AF & 0xFFFF, HL & 0xFFFF, DE & 0xFFFF, BC & 0xFFFF);
            }
            fprintf(outputfile,"\n");

            previousPC=PC;
            if (cpmswitchstate==0){
                  // nassys3 mode 
                  int numberofbytes=0;  // set to how many extra bytes to display
                if ( ( RAM(PC)& 0xC7) == 0xC7) { // a RST opcode
                    char disstr[100];
                    disstr[0]=0;
                    switch(RAM(PC) & 0x38 ){
                        case 0x00:   //  START
                            strcpy(disstr,"START");                        
                            break;
                        case 0x08:   //  RIN
                            strcpy(disstr,"RIN");                        
                            break;
                        case 0x10: {// RCAL
                            strcpy(disstr,"RCAL ");                        
                            numberofbytes=1; // extra byte for the call displacement
                            // calculate the jump address
                            char stemp[10];
                            // it works on the address after the displacement byte 
                            // also can be a negative 8 bits 
                            sprintf(stemp,"%4.4Xh",PC + (int8_t)RAM(PC+1) + 2 );
                            strcat(disstr,stemp);
                            break;
                        }
                        case 0x18:   // SCAL  - call routine 
                            numberofbytes=1; // the routine to call
                            strcpy(disstr,"SCAL ");                        
                            switch (RAM(PC+1)){
                                case 0x5B:  //MRET
                                    strcat(disstr,"MRET");                        
                                    break;
                                case 0x5C:  //SCALJ
                                    strcat(disstr,"SCALJ");                        
                                    break;
                                case 0x5D:  //TDEL
                                    strcat(disstr,"TDEL");                        
                                    break;
                                case 0x5E:  //FFLP
                                    strcat(disstr,"FFLP");                        
                                    break;
                                case 0x5F:  //MFLP
                                    strcat(disstr,"MFLP");                        
                                    break;
                                case 0x60:  //ARGS
                                    strcat(disstr,"ARGS");                        
                                    break;
                                case 0x61:  //KBD
                                    strcat(disstr,"KBD");                        
                                    break;
                                case 0x62:  //IN
                                    strcat(disstr,"IN");                        
                                    break;
                                case 0x63:  //INLIN
                                    strcat(disstr,"INLIN");                        
                                    break;
                                case 0x64:  //NUM
                                    strcat(disstr,"NUM");                        
                                    break;
                                case 0x66:  //TBCD3
                                    strcat(disstr,"TBCD3");                        
                                    break;
                                case 0x67:  //TBCD2
                                    strcat(disstr,"TBCD2");                        
                                    break;
                                case 0x68:  //B2HEX
                                    strcat(disstr,"B2HEX");                        
                                    break;
                                case 0x69:  //SPACE
                                    strcat(disstr,"SPACE");                        
                                    break;
                                case 0x6A:  //CRLF
                                    strcat(disstr,"CRLF");                        
                                    break;
                                case 0x6B:  //ERRM
                                    strcat(disstr,"ERRM");                        
                                    break;
                                case 0x6C:  //TX1
                                    strcat(disstr,"TX1");                        
                                    break;
                                case 0x6D:  //SOUT
                                    strcat(disstr,"SOUT");                        
                                    break;
                                case 0x6F:  //SRLX
                                    strcat(disstr,"SRLX");                        
                                    break;
                                case 0x70:  //SRLIN
                                    strcat(disstr,"SRLIN");                        
                                    break;
                                case 0x71:  //NOM
                                    strcat(disstr,"NOM");                        
                                    break;
                                case 0x72:  //NIM
                                    strcat(disstr,"NIM");                        
                                    break;
                                case 0x74:  //XKBD
                                    strcat(disstr,"XKBD");                        
                                    break;
                                case 0x76:  //UIN
                                    strcat(disstr,"UIN");                        
                                    break;
                                case 0x77:  //NNOM
                                    strcat(disstr,"NNOM");                        
                                    break;
                                case 0x78:  //NNIM
                                    strcat(disstr,"NNIM");                        
                                    break;
                                case 0x79:  //RLIN
                                    strcat(disstr,"RLIN");                        
                                    break;
                                case 0x7A:  //B1HEX
                                    strcat(disstr,"B1HEX");                        
                                    break;
                                case 0x7B:  //BLINK
                                    strcat(disstr,"BLINK");                        
                                    break;
                                case 0x7C:  //CPOS
                                    strcat(disstr,"CPOS");                        
                                    break;
                                case 0x7D:  //RKBD
                                    strcat(disstr,"RKBD");                        
                                    break;
                                case 0x7E:  //SP2
                                    strcat(disstr,"SP2");                        
                                    break;
                                case 0x7F:  //SCALI
                                    strcat(disstr,"SCALI");                        
                                    break;
                                case 0x80:  //PolyDos additions
                                    strcat(disstr,"DSIZE PolyDos");
                                    break;
                                case 0x81:
                                    strcat(disstr,"DRD PolyDos");
                                    break;
                                case 0x82:
                                    strcat(disstr,"DWR PolyDos");
                                    break;
                                case 0x83:
                                    strcat(disstr,"RDIR PolyDos");
                                    break;
                                case 0x84:
                                    strcat(disstr,"WDIR PolyDos");
                                    break;
                                case 0x85:
                                    strcat(disstr,"CFS PolyDos");
                                    break;
                                case 0x86:
                                    strcat(disstr,"LOOK PolyDos");
                                    break;
                                case 0x87:
                                    strcat(disstr,"ENTER PolyDos");
                                    break;
                                case 0x88:
                                    strcat(disstr,"COV PolyDos");
                                    break;
                                case 0x89:
                                    strcat(disstr,"COVR PolyDos");
                                    break;
                                case 0x8A:
                                    strcat(disstr,"CKER PolyDos");
                                    break;
                                case 0x8B:
                                    strcat(disstr,"CKBRK PolyDos");
                                    break;
                                case 0x8C:
                                    strcat(disstr,"CFMA PolyDos");
                                    break;
                                case 0x8D:
                                    strcat(disstr,"SSCV PolyDos");
                                    break;
                                case 0x8E:
                                    strcat(disstr,"JUMP PolyDos");
                                    break;
                                case 0x8F:
                                    strcat(disstr,"POUT PolyDos");
                                    break;
                                default:
                                    strcat(disstr,"????");
                                    break;
                            }
                            break;
                        case 0x20: // BRKPT
                            strcpy(disstr,"BRKPT");                        
                            break;
                        case 0x28: {// PRS
                            strcpy(disstr,"PRS ");
                            numberofbytes=1; // could be more as output a string till 0
                            int stringaddress = (PC+1)&0xFFFF;
                            while (RAM(stringaddress)!=0){
                                if (numberofbytes<60){
                                    if (RAM(stringaddress) < ' ' ){
                                        fprintf(outputfile,"string %2.2X\n",RAM(stringaddress));
                                        disstr[numberofbytes+3]='.';
                                    }
                                    else {
                                        disstr[numberofbytes+3]=RAM(stringaddress);
                                    }
                                    disstr[numberofbytes+4]=0;
                                }
                                if (numberofbytes>100){
                                    fprintf(outputfile,"string is over %d bytes \n",numberofbytes);
                                    break;
                                }
                                stringaddress++;
                                numberofbytes++;
                            }
                            break;
                        }
                        case 0x30: // ROUT
                            strcpy(disstr,"ROUT");                        
                            break;
                        case 0x38: // RDEl
                            strcpy(disstr,"RDEL");                        
                            break;
                        default:
                            strcpy(disstr,"????");                        
                            break;
                    }
                    fprintf(outputfile,"::");
                    if (numberofbytes>0){
                        int byteaddress=(PC+1)&0xFFFF;
                        int numbertoprint = numberofbytes&0x3; // restrict output to 3 bytes
                        fprintf(outputfile,"%4.4X: ",byteaddress);
                        while(numbertoprint>0){
                            fprintf(outputfile,"%2.2X ",RAM(byteaddress));
                            numbertoprint--;
                            byteaddress++;
                        }
                        if (numberofbytes>4){
                            fprintf(outputfile,".. ");
                        }
                        // output spaces for unused byte spaces
                        for(int i=4;i>numberofbytes;i--){
                            fprintf(outputfile,"   ");
                        }
                        fprintf(outputfile,"%*c",6,' ');
                    }
                    else{
                        // space out long way
                        fprintf(outputfile,"%*c",24,' ');
                    }
                    fprintf(outputfile,"%s\n",disstr);
                }
            }

        }
        else{
            if ( (previousPC>=tracestartaddress) && (previousPC <= traceendaddress) )  {
                // display this address as previous one was in the range
              fprintf(outputfile,"::%4.4X: > > >\n",PC);
            }
            previousPC=PC;
        }

    return lencmd;

}




// end of file


