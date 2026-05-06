The nasfiles folder is used to hold some programs that can be run on the emulator

These .nas files are Nascom ascii format files.  

( If you have a .cas see Neal Crooks convertor program  )
When loading .nas files they are loaded into "RAM" space  
if you don't want them "paged out" or written to then change their names to .rom.


* **m80memtp.nas** my memory test program  

https://github.com/dallday/map80memorytest  

this is loaded into 0xc80 

call it using  
E C80 1000 2000 


