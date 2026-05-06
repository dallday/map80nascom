MAP 80 Nascom Version 8.1 March 2026
========================================

    https://github.com/dallday/map80nascom

This emulator is designed to work like my Nascom2 setup with 
    MAP80 256k Ram card
    MAP80 VFC card
    CHS Clock card at port #D0-D4

Note as of 30 march 2026 (ver 8.1) it has been updated to add support for NASCOM4 SDcard interface.
This was provided by Neal Crook who designed the N4 system.
For more details on using it see his N4 documentation.


on start up it automatically loads to following files from the roms folder
* roms/nassys3.nas   - used without the -b, the NASSYS3 monitor rom
* roms/vfcrom0.nas   - used with the -b option, the rom file for the MAP80 VFC to boot into cpm

There are some other "roms" that can be loaded by specifing them in the command line used to call the program.  
Note; files end .rom will be loaded into their own Read only memory and will not be swapped out.


* roms/basic.rom              - Nascom basic loaded into ram at 0xE000.
* roms/PolyDos_2_Boot_ROM.rom - Polydos 2 boot rom loaded into ram at 0xD000

e.g. 
To load the basic rom enter
./map80nascom roms/basic.rom

To load polydos 2 enter
./map80nascom roms/PolyDos_2_Boot_ROM.rom -f disks/PD000.config

To run cpm 2.2 enter
./map80nascom -f disks/cpm001system22.config -b 



INSTALLATION
------------

The map80nascomlinux.zip contains a precompiled version for linux.
The program does depend upon SDL2 to run.
For Ubuntu it should be enough to install libsdl2-dev

e.g. `sudo apt install libsdl2-dev`

see otherdocs/map80librariesneeded.txt for more details

MAP 80 Nascom should compile on all platform with SDL support, but
has only been tested on Debian Linux.

To compile you may have to adapt the Makefile with the libraries you
need and their path, but generally it should be enough to simply run

    $ make

USAGE
-----

The emulator is designed to run from a terminal window.

This is MAP80 Nascom.  Usage: Debug/map80nascom {flags} files
        {flags}
           -i <file>        take serial port input from file (if tape led is on)
           -o <file>        send serial port output to file
           -m <file>        use <file> as monitor (default is nassys3.nal)
           -b               boot in cpm mode
           -f <configfile>  load floppy drive - repeat for up to 4 drives
           -s factor        change the window sizes - default factor is 2
           -t               output a trace of the Z80 opcodes executed ( also F2 )
           -l  start:end    limits the trace process to an address range
           -v               be verbose
           -x               use bios monitor when starting and stopped ( see biosmonitor readme )
       files                a list of nas files to load
        
Note: You can exit the emulator by pressing F4, closing either of the windows or by doing Control+c on the terminal.

The following keys are supported:

* F1 - Triggers an NMI 
* F2 - Turns on/off disassembler trace (like -t option)
* F3 - resets the position of the serial input 
* F4 - exits the emulator
* F5 - toggles between stupidly fast and "normal" speed
* F6 - force serial input on
* F9 - resets the emulated Nascom
* F10 - toggles between "raw" and "natural" keyboard emulation
* END - leaves a nascom screen dump in `screendump`

You can add files to be loaded by providing them as arguments at the end of
the line.
For example to run *Pac Man*, run

    $ ./map80nascom progs/pacman.nas

and type `E1000` in the Nascom 2 window. Control with arrow keys. You might switch to the "raw" keyboard" mode by pressing F10 to make the controls work better.

NASSYS mode:
------------

The emulator will display the NASCOM 2 style display and a status display.

The emulator conveniently dumps the memory state in `memorydump.nas`
upon exit so you can resume execution later on.

CPM Mode:
---------

This is activated by using the -b option
The emulator will display the MAP80 VFC display and a status display.

It then uses VFC rom mapped to address 0 to load the first sector from drive 0
into memory address 0x1000 and then executes it.

The emulator works with the cpm2.2 and cpm3 floppys in the disks folder.

Serial Input and Output:
-----------------------

The `-i` option allows you to have a serial input file - normally a .cas file.
This will be used as input when the R command is used in NASSYS3. Note:- F3 resets the input file.
The name of and the position in the input file is show in the status window.

The `-o` option allows you to specify where serial output will be saved.
All serial output is appended to the file, e.g. the W command in NASSYS3.
If no file is specified then the output is lost.
The output file may be fed back in on a subsequent launch via the `-i` option.
The name of and the position in the output file is show in the status window.

The files are not locked except during actual read or write operations.
This allows you to change the files at any time.

You can also use the same file as input and output. 

In linux it is possible to use the ln command to link an input file to a standard
filename.

Floppy Discs
------------

The emulator is able to use a number of virtual floppy formats.

The details of the image are stored in a .config file which details

* where the actual image file is 
* the format of the image file

The format of the image file is specified in the same way as Flash Floppy does in it's IMG.CFG file.

See the Disks folder for more details.

The `-f` option specifies which configs to use.
The first entry is used for drive 0, the second is used for drive 1 etc.,
It can handle up to 4 drives at present.


CREDITS
-------

A very crucial part of Virtual Nascom is the excellent Z-80 emulator
from Yaze, Copyright (C) 1995,1998  Frank D. Cringle.

It was build on the Virtual Nascom Version 2.0, 2020-01-20
by Tommy Thorn.

* Git repository: http://github.com/tommythorn/virtual-nascom.git

A big thank you goes to John Hanson and his Xbeaver project, for providing 
some of the details needed for the the memory management and floppy disc controller.

* http://www.cccplusplus.co.uk/xbeaver.html

And thanks to Neal Crook for his support and the copy of PolyDos2

* https://github.com/nealcrook/nascom


KNOWN ISSUES
------------
* Tommy Thorn said that *Galaxy Attack* did not work on Virtual Nascom.

    It does work on map80nascom :)

Let me know if you find any issues with the program.

david_github@davjanbec.co.uk




