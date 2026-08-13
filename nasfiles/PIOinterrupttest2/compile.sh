#!/bin/bash
# compile and copy the program to the vnascom emulator
# build etc., are in the ~/bin directory
build testPIOint2
# convert it to a cassette file
nascon testPIOint2.bin testPIOint2.cas -org c80
nascon testPIOint2.bin testPIOint2.nas -org c80 -csum

