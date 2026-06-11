** **NAS style files**

The nasfiles folder is used to hold some programs that can be run on the emulator.

These .nas files are Nascom ascii format files.  

If you have a .cas see Neal Crooks convertor program cas2nas. 
    https://github.com/nealcrook/nascom/tree/master/converters 
    
When loading .nas files they are loaded into "RAM" space  
if you don't want them "paged out" or written to then change their names to .rom.


* **m80memtp.nas**
Is my memory test program.
Use the .nas to load it at the start or the .cas to specify it using the -i option.
Run using E C80 8000 9000 
See https://github.com/dallday/map80memorytest

* **Pacman2.nas**
Game of Pacman 
Load using 
./map80nascom roms/Pacman2.nas
then 
E 1000 

* **GALAXY.NAS**
Game like Space Invaders 
Load using 
./map80nascom roms/GALAXY.NAS
then 
E 1000 


