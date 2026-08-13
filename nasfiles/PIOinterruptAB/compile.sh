#!/bin/bash
# compile and copy the program to the vnascom emulator
# build etc., are in the ~/bin directory
build testPIOintAB
# convert it to a cassette file
nascon testPIOintAB.bin testPIOintAB.cas -org c80
nascon testPIOintAB.bin testPIOintAB.nas -org c80 -csum

