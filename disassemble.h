/***
 *  Z80 Disassembler Header file
 *
 ***/


// process an opcode
extern int disassembleline (unsigned int address, unsigned  char * bindata, char * returnedline);

// process a line in the program 
// replaced above to include limits and nassys code.
extern int disassembleprogram(FASTREG PC,
                        FILE * outputfile,
                        int showregisters,
                        FASTREG startaddress,
                        FASTREG endaddress,
                        FASTREG AF,
                        FASTREG BC,
                        FASTREG DE,
                        FASTREG HL,
                        FASTREG SP);


// end of file

