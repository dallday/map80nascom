The roms folder is used to keep all the Z80 code needed to run the emulator,
plus some additional nas and rom files. 

These .nas and .rom files are Nascom ascii format files.

When loading .rom files they are loaded into "ROM" space and therefore will not be paged out or written to. If you want them loaded into "RAM" then change their names to .nas.

There are files that are automatically loaded into their own "ROM" space at start up.
The emulator checks what is needed for the mode ( -n, -v or -4 )
It will fail to start if the ones it needs are not present.
Use the -d 1 option if it does not seem to be working.

* **nassys3.nas** NASSYS3.  Extracted from my system. 
    This is loaded into address 0 if the -n options is used.
    You can specify a different monitor using the -m option.

* **vfcrom0.nas** The boot rom for the vfc card.
    The VFC vfcrom0.nas was extracted from my system.
    This is loaded into the MAP80 VFC card rom area and activated if the -v option is used.

* **z80_sboot_rom.nas** the N4 boot rom.
    Thanks to Neal Crook for this file
    It is loaded into address 0x1000 
    It is activated for the -4 mode.

* **nascom4_sdcard.img** the N4 SD card image
    Thanks to Neal Crook for this.
    It is attached as the SD image for N4
    You can use a different image by using the -c option.
    
The are optional files that can be specified on the command line to load.

* **basic.rom** Nascom2 Basic. Thanks to http://www.nascomhomepage.com/ 

* **PolyDos_2_Boot_ROM.rom** The boot rom for PolyDos 2.
    Thanks to Neal Crook - https://github.com/nealcrook/nascom/tree/master/PolyDos/rom
    see for documentation.
            https://github.com/nealcrook/nascom/tree/master/PolyDos  
    
    Load Polydos using 
    ./map80nascom -f disks/PD000.config -f disks/PD600.config roms/PolyDos_2_Boot_ROM.rom
    start Polydos using  
    E D000  
    You should be asked "Boot which drive?"  
    press 0  
    and you should the Polydos prompt.  




