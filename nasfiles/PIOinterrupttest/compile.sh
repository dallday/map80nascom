#!/bin/bash
# compile and copy the program to the vnascom emulator
# build etc., are in the ~/bin directory
build testPIOint
# convert it to a cassette file
nascon testPIOint.bin testPIOint.cas -org c80
nascon testPIOint.bin testPIOint.nas -org c80 -csum
# copy the cas file to the emulator
#cp testPIOint.cas /home/david/dosbox/cdrive/vnascom/testPIOint.cas
#cp testPIOint.nas /home/david/nascom/emulators/map80nascom/testPIOint.nas

