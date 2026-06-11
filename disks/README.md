# Floppy discs
------------

The format of the floppy disc image is specified in the .config file.

The format of this file is similar to the Flash Floppy IMG.CFG file used for the Gotek images.

    https://github.com/keirf/FlashFloppy/wiki/IMG.CFG-Configuration-File

although not all the options have been implimented.

# The disks provided
--------------------

* PD000.config  - Polydos 2 boot disk
* PD600.config  - Polydos 2 program disk
* cpm001system22.config - CPM 2.2 boot disk
* cpm3.config   - original CPM 3 boot disk
* cpm3seq.config - CPM3 boot disk with Y2k fix.
* cpmutils.config - some utilites for cpm3
* dummy.config   - a sample config file

# Config file Parameters
----------------------
    see diskconfig.txt

Comments can be included by the first non space entry on the line being a #

    # image config file for CPM2 disc

* imagefilename=filename

    the name of the actual image file to use.
    
* cyls = 1 to 255

    the number of cylinders ( aka tracks ) in the image
    
* heads = 1 or 2

    the number of heads ( aka sides ) in the image
    
* secs = 0 to 255

    the number of sectors per track
    
* bps = 128 | 256 | 512 | 1024 | 2048 | 4096 | 8192

    The number of bytes per sector
    
* id = 0-255 

    The ID of the first sector on each track
    Successive sectors are numbered sequentially upwards

* file-layout = interleaved* | sequential | reverse-sideN | sides-swapped

    ** Image file track layout
    ** Multiple values can be specified, separated by commas (eg. sequential,reverse-side1)
    ** sequential: Sequential cylinder ordering: all side 0, then side 1
    ** interleaved: Interleaved cylinder ordering: c0s0, c0s1, c1s0, c1s1, ...
    ** reverse-sideN: Side-N cylinders are in reverse order (high to low) (N=0,1)
    ** sides-swapped: Sides 0 and 1 ordering is swapped in the image file

    see the disks folder for examples of the config file


