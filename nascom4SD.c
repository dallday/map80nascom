/*  SDcard controller integrated into "NASCOM 4" FPGA

    Neal Crook, July 2021

*/

#include <stdlib.h>            // std libraries
#include <stdio.h>
#include <assert.h>

#include "options.h"           // defines the options to use
#include "nascom4SD.h"         // define the SDcard stuff

// Logical block address written through SDLBA2/1/0
static unsigned int lba;

// Track state
// 0 no image file
// 1 initialising
// 2 idle
// 3 read command
// 4 write command
static int state = 0;

// How many times we've polled waiting
static int poll;

// On write command, sector buffer is filled with 0 and SDindex is set to 0. Write data
// goes to the buffer and writes increment SDindex. Finally, the buffer is flushed to the disk image.
// On read command, sector buffer is loaded from disk image and SDindex is set to 0. Read data
// comes from the buffer and reads increment SDindex.
static unsigned char sector[512];
static int index;

static FILE * sd_file;

// print 512-byte buffer in hex and ASCII
// Buffer is in sector[] and its start address
// is addr.
void dump_buffer(int addr)
{
    for (int i = 0; i<16; i++) {
        fprintf(stdout,"%08x: ", addr+i*32);
        for (int j = 0; j<32; j++) {
            if (j%8 == 0) {
                fprintf(stdout," ");
            }
            fprintf(stdout,"%02x ",sector[i*32+j]);
        }
        fprintf(stdout," ");
        for (int j = 0; j<32; j++) {
            if (j%8 == 0) {
                fprintf(stdout," ");
            }
            if ((sector[i*32+j] < 0x7f) && (sector[i*32+j] > 0x1f)) {
                fprintf(stdout,"%c", sector[i*32+j]);
            }
            else {
                fprintf(stdout,".");
            }
        }
        fprintf(stdout,"\n");
    }
}


extern int SDMountDisk(char * filename)
{
    sd_file = fopen(filename, "r+b");
    if (sd_file == NULL) {
        fprintf(stdout, "SDcard failed to load '%s', ignoring it.\n", filename);
        perror(filename);
    }
    else {
        fprintf(stdout, "Mount SDcard, image file '%s'\n", filename);
        state = 1;
    }
    return 0;
}


// [NAC HACK 2021Jul31] implement busy bit based on index so that code does not need a counter?
// .. need to test on real hardware.
// for read: when polling for data status goes A0->E0 when byte available, and to 80 at end of block
// for write: when polling for space status goes 20->A0 when it can accept a byte and to 80 at end of block

void outPortSD (unsigned int port, unsigned int wdata)
{
    int retvar;
    //fprintf(stdout,"INFO outPortSD with port=0x%02x wdata=0x%02x poll=%d state=%d index=%d lba=0x%06x\n", port, wdata, poll, state, index, lba);
    switch (port) {
    case SDDATA:
        switch (state) {
        case 0: case 1: case 2: case 3:
            fprintf(stdout,"ERROR attempt to write to SD data during SD block read\n");
            break;
        case 4: // in write
            if (poll == 3) {
                poll = 0;
                if (index < 512) {
                    sector[index++] = wdata;
                    if (index == 512) {
                        state = 2; // back to idle.
                        // commit the data
                        if (fseek(sd_file, lba<<9, SEEK_SET)) {
                            fprintf(stdout,"ERROR seek to SD address 0x%x for write failed\n", lba<<9);
                        }
                        else {
                            retvar = fwrite(sector, sizeof(sector), 1, sd_file);
                            assert(retvar != -1);
                            fprintf(stdout,"WROTE THE DATA for address 0x%x and got retvar %d\n", lba<<9, retvar);
                            //                            dump_buffer(lba<<9);
                        }
                    }
                }
                else {
                    fprintf(stdout,"ERROR attempt to write too much data to SD data\n");
                }
            }
            else {
                fprintf(stdout,"ERROR attempt to write to SD data while busy\n");
            }
            break;
        }
        break;
    case SDCONTROL:
        switch (state) {
        case 0:
            fprintf(stdout,"ERROR attempt to write to SD control but no SD image file\n");
            break;
        case 1:
            fprintf(stdout,"ERROR attempt to write to SD control but still initialising\n");
            break;
        case 2:
            poll = 0;
            switch (wdata) {
            case 0: // read command
                //fprintf(stdout,"INFO SD READ: port=0x%02x wdata=0x%02x poll=%d state=%d index=%d lba=0x%06x (0x%08x)\n", port, wdata, poll, state, index, lba, lba<<9);
                if (fseek(sd_file, lba<<9, SEEK_SET)) {
                    fprintf(stdout,"ERROR seek to SD offset address 0x%x for read failed\n", lba<<9);
                    for (index = 0; index < 512; index++) {
                        sector[index] = 0xff;
                    }
                }
                else {
                    retvar = fread(sector, sizeof(sector), 1, sd_file);
                    assert(retvar != -1);
                }
                //                dump_buffer(lba<<9);
                state = 3;
                index = 0;
                break;
            case 1: // write command
                //fprintf(stdout,"INFO SD WRITE: port=0x%02x wdata=0x%02x poll=%d state=%d index=%d lba=0x%06x (0x%08x)\n", port, wdata, poll, state, index, lba, lba<<9);
                state = 4;
                index = 0;
                break;
            default:
                fprintf(stdout,"ERROR unknown SD command 0x%02x\n", wdata);
                break;
            }
            break;
        case 3:
            fprintf(stdout,"ERROR attempt to write to SD control during SD block read\n");
            break;
        case 4:
            fprintf(stdout,"ERROR attempt to write to SD control during SD block write\n");
            break;
        default:
            fprintf(stdout,"ERROR write to SD control when state is %d\n",state);
            break;
        }
        break;
    case SDLBA0: // SDLBA0
        lba = (lba & 0xffff00) | (0xff & wdata);
        //fprintf(stdout,"INFO outPortSD port=0x%02x wdata=0x%02x now lba=0x%06x\n", port, wdata, lba);
        break;
    case SDLBA1: // SDLBA1
        lba = (lba & 0xff00ff) | ((0xff & wdata) << 8);
        //fprintf(stdout,"INFO outPortSD port=0x%02x wdata=0x%02x now lba=0x%06x\n", port, wdata, lba);
        break;
    case SDLBA2: // SDLBA2
        lba = (lba & 0x00ffff) | ((0xff & wdata) << 16);
        //fprintf(stdout,"INFO outPortSD port=0x%02x wdata=0x%02x now lba=0x%06x\n", port, wdata, lba);
        break;
    }
}


int inPortSD(unsigned int port)
{
    //fprintf(stdout,"INFO inPortSD with port=0x%02x poll=%d state=%d index=%d lba=0x%06x\n", port, poll, state, index, lba);
    switch (port) {
    case SDDATA:
        switch (state) {
        case 0: case 1: case 2:
            return 0xff;
        case 3: // in read
            if (poll == 3) {
                poll = 0;
                if (index == 511) {
                    state = 2; // final read then back to idle
                }
                if (index < 512) {
                    return sector[index++];
                }
                else {
                    fprintf(stdout,"ERROR attempt to read too much data from SD block\n");
                    return 0xff;
                }
            }
            else {
                fprintf(stdout,"ERROR attempt to read SD block when data not yet available\n");
                return 0xff;
            }
        case 4: // in write
            fprintf(stdout,"ERROR attempt to read SD data during write\n");
            return 0xff;
        }
        break; // unreachable
    case SDCONTROL:
        switch (state)
            {
            case 0: // busy and always will remain so
                return 0x90;
            case 1: // count polls and initialise
                poll++;
                if (poll == 16)
                    state = 2;
                return 0x90;
            case 2: // idle.
                return 0x80;
            case 3: // in read
                if (poll < 3)
                    poll++;
                if (poll == 3) {
                    return 0xe0; // data available
                }
                else {
                    return 0xa0; // still waiting
                }
            case 4: // in write
                if (poll < 3)
                    poll++;
                if (poll == 3) {
                    return 0xa0; // space available
                }
                else {
                    return 0x20; // still waiting
                }
            }
        break; // unreachable
    }
    return 0x00; // return default rdata
}

// end of code
