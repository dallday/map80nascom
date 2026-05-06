/* Header file for the instruction set simulator.
   Copyright (C) 1995  Frank D. Cringle.

This file is part of yaze - yet another Z80 emulator.

Yaze is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <stdio.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "options.h"  //defines the options to use

// these are in map80nascom.h but . . . . .
extern int traceon; // set to 1 to trace z80
extern int tracestartaddress;    // trace will only show results when the PC is within this range
extern int traceendaddress;      //  end address of the trace range ( 0 to 0xFFFF )

#if UCHAR_MAX == 255
typedef uint8_t BYTE;
#else
#error Need to find an 8-bit type for BYTE
#endif

#if USHRT_MAX == 65535
typedef uint16_t WORD;
#else
#error Need to find an 16-bit type for WORD
#endif

/* FASTREG needs to be at least 16 bits wide and efficient for the
   host architecture */
#if UINT_MAX >= 65535
typedef unsigned int	FASTREG;
#else
typedef unsigned long	FASTREG;
#endif

/* FASTWORK needs to be wider than 16 bits and efficient for the host
   architecture */
#if UINT_MAX > 65535
typedef unsigned int	FASTWORK;
#else
typedef unsigned long	FASTWORK;
#endif

/* NMI controls */
extern int singleStep; // set to 4 to execute some instructions before triggering NMI
extern int NMI_flag;   // set to 1 to trigger NMI

/* two sets of accumulator / flags */
extern WORD af[2];
extern int af_sel;

/* two sets of 16-bit registers */
extern struct ddregs {
	WORD bc;
	WORD de;
	WORD hl;
} regs[2];
extern int regs_sel;

extern WORD ir;
extern WORD ix;
extern WORD iy;
extern WORD sp;
extern WORD pc;
extern WORD IFF;

// fix DA see options.h for these definitions
// this defines how much memory the Z80 can see in K
//#ifndef MEMSIZE
//#define MEMSIZE 64
//#endif

// fix DA - +1 not really needed as change putword / getword
// used to work since 0 to 0x7ff was ROM
//extern BYTE ram[MEMSIZE*1024+1];  // The +1 location is for the wraparound GetWord
// if using MMU then we have a set of pointers to point to ram
// each entry points to the start of that page
// will be 2k if RAMPAGESIZE is 2
extern BYTE *rampagetable[RAMPAGETABLESIZE];
// see map80ram.h

// DA Fix ram as the old memory space
// virutalram replaced it
// but this module always access ram via the rampagetable table
// so does not need to see ram or virtual ram
//extern BYTE virutalram[VIRTUALRAMSIZE*1024];
//extern BYTE ram[RAMSIZE*1024];

// fix DA
// ramromtable is set to 0 if you can write to it
// otherwise a write attempt is just ignored
// i.e. that area is a ROM
extern int ramromtable[RAMPAGETABLESIZE];

// ram lock table is set to 0 if you can change it's pointer in rampagetable
// otherwise the rampagetable will not be changed
// acts like the N2 ram disable line.
// normally this would be linked to a seperate 2k block and not the nrmal ram area.
extern int ramlocktable[RAMPAGETABLESIZE];

#ifdef DEBUG
extern volatile int stopsim;
#endif

extern FASTWORK simz80(FASTREG PC, int, int (*)());

#define FLAG_C	1
#define FLAG_N	2
#define FLAG_P	4
#define FLAG_H	16
#define FLAG_Z	64
#define FLAG_S	128

#define SETFLAG(f,c)	AF = (c) ? AF | FLAG_ ## f : AF & ~FLAG_ ## f
#define TSTFLAG(f)	((AF & FLAG_ ## f) != 0)

#define ldig(x)		((x) & 0xf)
#define hdig(x)		(((x)>>4)&0xf)
#define lreg(x)		((x)&0xff)
#define hreg(x)		(((x)>>8)&0xff)

#define Setlreg(x, v)	x = (((x)&0xff00) | ((v)&0xff))
#define Sethreg(x, v)	x = (((x)&0xff) | (((v)&0xff) << 8))

// fix DA - original was for 4k pages
//#define RAM(a)		*(pagetable[((a)&0xffff)>>12]+((a)&0x0fff))
// need to define based on 2k pages and for a bigger virtual space
// This defines how we work out the actual address of the byte we want in memory
//      the entry in the rampagetable + the offset

#define RAM(a)		*(rampagetable[((a)>>RAMPAGESHIFTBITS) & RAMPAGETABLESIZEMASK] + ((a) & RAMPAGEMASK) )

static inline unsigned char
GetBYTE(uint16_t a)
{
// fix DA - change to use the RAM macro
//    return ram[a];
      return RAM(a);
}

/* Fast write inside [3K; 64K-8K]

   NOTICE: This is dependent on the assumption that the 8KB Basic ROM
   is present
*/

// Fix DA does not seem to be defined anywhere
// void slow_write(unsigned int a, unsigned char v);

static inline void
PutBYTE(uint16_t a, uint16_t v)
{
// fix DA removed the basic rom for now
// need to look to a more subtle way of setting it
// once paging is sorted
// use a lock table to decide if you can write to that area
// simplier size to the pagetable
// use the RAM macro

//    if (0x800 <= a && a < 0xE000)
//        ram[a] = v;
      // check if ram rom table = 0
//      RAM(a) = v;

/*
      fprintf(stdout, "address [%04x] [%04x] = [%02x] value [%02x] \n",
           a,
           (a >> RAMPAGESHIFTBITS) & RAMPAGETABLESIZEMASK,
           ramromtable[ (a >> RAMPAGESHIFTBITS) & RAMPAGETABLESIZEMASK],
            v );
*/

      if ( ramromtable[ (a >> RAMPAGESHIFTBITS) & RAMPAGETABLESIZEMASK] == 0 ) {
        RAM(a) = v;
      }
}
// this was in original zaye code
/*#define PutBYTE(a, v)	RAM(a) = v*/


// original from yaze
// macro to create inline code
#define GetWORD(a)	(RAM(a) | (RAM((a)+1) << 8))
// fix DA - removed the virtual-nascom version 2 code
//
// Note, works even for 0xFFFF because we maintain ram[0] === ram[0x10000]
// Note: http://blog.regehr.org/archives/959
// fix DA change to use PutBYTE - slower but . . . .
/*
static inline uint16_t GetWORD(uint16_t a)
{
    uint16_t res;
    memcpy(&res, ram+a, 2);
    return res;
}
*/


static inline void PutWORD(unsigned a, uint16_t v)
{
// fix DA change to use PutBYTE - slower but . . . .
// need to look to a more subtle way of setting it
// once paging is sorted
//    if (0x800 <= a && a < 0xE000 - 1)
//        memcpy(ram+a, &v, 2);
// do put twice to avoid problems when address 0xffff will write second byte to 0x0000
    PutBYTE(a, ((int)(v) & 255));
    PutBYTE((a)+1, (v) >> 8);

}

#ifndef BIOS
extern int in(unsigned int);
extern void out(unsigned int, unsigned char);
#define Input(port) in(port)
#define Output(port, value) out(port,value)
#else
/* Define these as macros or functions if you really want to simulate I/O */
#define Input(port)	0
#define Output(port, value)
#endif

