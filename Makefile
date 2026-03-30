# Makefile for map80 Nascom

# CC must be an C99 compiler
CC=gcc -std=c99

# full speed or debugging to taste
#Full speed use this line
OPTIMIZE=-O2
#Debugging use this line
#OPTIMIZE=-g

#WARN=-Wmost -Werror
WARN=-Wall -Wno-parentheses

CFLAGS=$(OPTIMIZE) $(WARN) $(shell sdl2-config --cflags)

#map80nascom: map80nascom.o font.o simz80.o nasutils.o ihex.o map80VFCfloppy.o display.o map80ram.o map80VFCdisplay.o

map80nascom: map80nascom.o serial.o chsclockcard.o cpmswitch.o  disassemble.o statusdisplay.o display.o  font.o  map80ram.o  map80VFCcharRom1.o  map80VFCdisplay.o  map80VFCfloppy.o  nasutils.o  sdlevents.o  simz80.o  utilities.o  biosmonitor.o nascom4SD.o
	$(CC) $(CWARN) $^ -o $@ $(shell sdl2-config --libs)

clean:
	rm -f *.o *~ core
