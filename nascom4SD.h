/*  SDcard controller integrated into "NASCOM 4" FPGA

    Neal Crook, July 2021

    data sheet:
    https://github.com/nealcrook/multicomp6809/blob/master/multicomp/Components/SDCARD/sd_controller.vhd

-- Port    Register
-- 0x10    SDDATA        read/write data
-- 0x11    SDSTATUS      read
-- 0x11    SDCONTROL     write
-- 0x12    SDLBA0        write-only
-- 0x13    SDLBA1        write-only
-- 0x14    SDLBA2        write-only (only bits 6:0 are valid)
--
-- For both SDSC and SDHC (high capacity) cards, the block size is 512bytes (9-bit value) and the
-- SDLBA registers select the block number. SDLBA2 is most significant, SDLBA0 is least significant.
--
-- For SDSC, the read/write address parameter is a 512-byte aligned byte address. ie, it has 9 low
-- address bits explicitly set to 0. 23 of the 24 programmable address bits select the 512-byte block.
-- This gives an address capacity of 2^23 * 512 = 4GB .. BUT maximum SDSC capacity is 2GByte.
--
-- The SDLBA registers are used like this:
--
-- 31 30 29 28.27 26 25 24.23 22 21 20.19 18 17 16.15 14 13 12.11 10 09 08.07 06 05 04.03 02 01 00
--+------- SDLBA2 -----+------- SDLBA1 --------+------- SDLBA0 --------+ 0  0  0  0  0  0  0  0  0
--
-- For SDHC cards, the read/write address parameter is the ordinal number of 512-byte block ie, the
-- 9 low address bits are implicity 0. The 24 programmable address bits select the 512-byte block.
-- This gives an address capacity of 2^24 * 512 = 8GByte. SDHC can be upto 32GByte but this design
-- can only access the low 8GByte (could add SDLBA3 to get the extra address lines if required).
--
-- The SDLBA registers are used like this:
--
-- 31 30 29 28.27 26 25 24.23 22 21 20.19 18 17 16.15 14 13 12.11 10 09 08.07 06 05 04.03 02 01 00
--  0  0  0  0  0  0  0  0+---------- SDLBA2 -----+------- SDLBA1 --------+------- SDLBA0 --------+
--
-- The end result of all this is that the addressing looks the same for SDSC and SDHC cards.
--
-- SDSTATUS (RO)
--    b7     Write Data Byte can be accepted
--    b6     Read Data Byte available
--    b5     Block Busy
--    b4     Init Busy
--    b3     Unused. Read 0
--    b2     Unused. Read 0
--    b1     Unused. Read 0
--    b0     Unused. Read 0
--
-- SDCONTROL (WO)
--    b7:0   0x00 Read block
--           0x01 Write block
--
--
-- To read a 512-byte block from the SDCARD:
-- Wait until SDSTATUS=0x80 (ensures previous cmd has completed)
-- Write SDLBA0, SDLBA1 SDLBA2 to select block index to read from
-- Write 0 to SDCONTROL to issue read command
-- Loop 512 times:
--     Wait until SDSTATUS=0xE0 (read byte ready, block busy)
--     Read byte from SDDATA
--
-- To write a 512-byte block to the SDCARD:
-- Wait until SDSTATUS=0x80 (ensures previous cmd has completed)
-- Write SDLBA0, SDLBA1 SDLBA2 to select block index to write to
-- Write 1 to SDCONTROL to issue write command
-- Loop 512 times:
--     Wait until SDSTATUS=0xA0 (block busy)
--     Write byte to SDDATA
--
-- At HW level each data transfer is 515 bytes: a start byte, 512 data bytes,
-- 2 CRC bytes. CRC need not be valid in SPI mode, *except* for CMD0.
--
-- SDCARD specification can be downloaded from
-- https://www.sdcard.org/downloads/pls/
-- All you need is the "Part 1 Physical Layer Simplified Specification"

*/

//check if we have defined all the entries else where
// not really required as no varibles defined.
#ifndef SD_DEFINED_H
#define SD_DEFINED_H

//Ports
#define SDDATA    (0x10)
#define SDSTATUS  (0x11)
#define SDCONTROL (0x11)
#define SDLBA0    (0x12)
#define SDLBA1    (0x13)
#define SDLBA2    (0x14)

extern int SDMountDisk(char * filename); // call to mount SDcard image

// global variables to allow for debug etc.,
//extern int vfcfloppydebug;
//extern int vfcfloppydisplaysectors;

void outPortSD(unsigned int port, unsigned int wdata);
int inPortSD(unsigned int port);


#endif

// end of file
