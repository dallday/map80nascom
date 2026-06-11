MAP 80 Nascom Version 9 June 2026
========================================

    https://github.com/dallday/map80nascom

This emulator is designed to work like my Nascom2 setup with 
    MAP80 256k Ram card
    MAP80 VFC card
    CHS Clock card at port #D0-D4 or ports #88-#8B
    but also supports the 
    Emulation of Neal Crook's NASCOM 4, including the N4 SD card interface

The program is called with various optionla parameters

The default options are to run as Nascom 2 with extended memory.

USAGE
-----

The emulator is designed to run from a terminal window.

The command is 
map80nascom {flags} files
        {flags}
        -n           emulator Nascom 2, the default.
        -v           emulate VFC boot for CPM.
        -4           emulate the N4.
        -c <file>    use this file as the N4 SD card image.
        -m <file>    use <file> as monitor (default is rom/nassys3.rom).
        -i <file>    take serial port input from file (if tape led is on).
        -o <file>    send serial port output to file.
        -f <configfile> load floppy drive - repeat for up to 4 drives.
        -s factor    change the window sizes - default factor is 2.
        -t           output a trace of the Z80 opcodes executed ( also F2 ).
        -l start:end limits the trace process to with an address range.
        -d <level>   debug level : 1 is verbose see below for more details.
        -x           use bios monitor when starting and stopped.
        files        a list of nas files to load.

Note: You can exit the emulator by pressing F4, closing either of the windows or by doing Control+c on the terminal.

Note: It has been updated to add support for NASCOM4 SDcard interface.
        This was provided by Neal Crook who designed the N4 system.
        For more details on using it see his N4 documentation.
        https://github.com/nealcrook/nascom/blob/master/nascom4/doc/nascom4_handbook.pdf


ROMS loaded
-----------

on start up it automatically loads to following files from the roms folder
* roms/nassys3.nas       - used with the -n option, the NASSYS3 monitor rom
* roms/vfcrom0.nas       - used with the -v option, the rom file for the MAP80 VFC to boot into cpm
* roms/z80_sboot_rom.nas - used with the -4 option, the rom for the N4 boot loader

It will try and load the SD image used by the N4 mode 
* roms/nascom4_sdcard.img  

Note: at present you will need to unpack the imag from roms/nascom4_sdcard.img.zip

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
./map80nascom -f disks/cpm001system22.config -v


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

See above for the options.

You can add files to be loaded by providing them as arguments at the end of
the line.
For example to run *Pac Man*, run

    $ ./map80nascom progs/pacman.nas

    and type `E1000` in the Nascom 2 window. Control with arrow keys. 
    You might switch to the "raw" keyboard" mode by pressing F9 to make the controls work better.
      
Note: You can exit the emulator by pressing F4, closing either of the windows or by doing Control+c on the terminal.

Bios Monitor (option -x)
------------------------

If you use the -x options then the emulator will stop in the terminal window after all roms etc., have been loaded.
**Make sure you select the terminal window to enter the commands**

The following commands are available which try to be similar to those in NASSYS3.

        D xxxx YYYY disassemble code address xxxx to yyyy
        E xxxx      to start Z80sim from address xxxx 
        O xx yy     output value yy on 'port' xx
        Q xx        query input from 'port' xx
        R           toggles the disassemble trace on/off
        S xxxx      to single step from address xxxx or last stop address.
        T xxxx yyyy output memory from address xxxx to yyyy
        X           to exit emulator
        Z           to display the current memory mapping 

In Emulation mode
-----------------

The following keys are supported:
**Make sure you select the NASCOM or VFC window to use these buttons**

* F1 - Triggers an NMI 
* F2 - does N4 style warm reset of the emulated Nascom
* F3 - resets the emulated Nascom
* F4 - exits the emulator (press shift to exit to Bios Monitor)
* F5 - resets the position of the serial input 
* F6 - force serial input on
* F7 - Turns on/off disassembler trace
* F8 - toggles between stupidly fast and "normal" speed
* F9 - toggles between "raw" and "natural" keyboard emulation
* END - leaves a nascom screen dump in `screendump` 


NASSYS mode (option -n)
-----------------------

The emulator will display the NASCOM 2 style display and a status display.

The emulator conveniently dumps the memory state in `memorydump.nas`
upon exit so you can resume execution later on.

VFC CPM Mode (option -v)
------------------------

This is activated by using the -v option

The emulator will display the MAP80 VFC display and a status display.

It then uses VFC rom mapped to address 0 to load the first sector from drive 0
into memory address 0x1000 and then executes it.

The emulator works with the cpm2.2 and cpm3 floppys in the disks folder.

Nascom 4 Mode (option -4)
-------------------------

Using the -4 option the emulator can emulate many of the features of the N4.  

The SD card needs to be specified to get the functions to work  
see roms/nascom4_sdcard.img.zip  
you will need to unzip the image into the rom folder.  

See Neal Crook's guide at https://github.com/nealcrook/nascom/tree/master/nascom4  



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
It can handle up to 4 drives.  

Debugging info ( option -d)
---------------------------

the -d options are 

* 1 - the verbose mode
* 2 - display nascom areas paging in and out
* 3 - show the CHS Clock debug
* 4 - display the N4 Port changes
* 5 - show the VFC paging in and out
* 6 - show the VFC floppy drive commands
* 7 - show the VFC floppy disc sectors

If you use the -d you must include at least 1 option but you can include as many as you want

e.g. 
-d 1245


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
and for supplying the Nascom 4 code

* https://github.com/nealcrook/nascom


KNOWN ISSUES
------------
* Tommy Thorn said that *Galaxy Attack* did not work on Virtual Nascom.

    It does work on map80nascom :)

Let me know if you find any issues with the program.

david_github@davjanbec.co.uk




