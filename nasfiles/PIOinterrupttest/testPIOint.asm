; ---------------------------------------------------------
; I/O port equates
; ---------------------------------------------------------
PIOA_DATA:   EQU 04H
PIOB_DATA:   EQU 05H
PIOA_CTRL:   EQU 06H
PIOB_CTRL:   EQU 07H

; ---------------------------------------------------------
; IM2 vector table page/byte
; ---------------------------------------------------------
VTAB_HI:     EQU 10H          ; I register = 10H -> table lives at 1000H
VTAB_LO:     EQU 00H          ; vector byte given to PIO (D0 must = 0)
VTAB_ADDR:   EQU 1000H        ; address of the pointer table entry

; ---------------------------------------------------------
; Main program
; ---------------------------------------------------------
            ORG 0C80H
START:      DI

            ; Build the IM2 pointer table entry in code, not with DW
            LD   HL, ISR         ; HL = address of service routine
            LD   (VTAB_ADDR), HL ; store it at 1000H/1001H
                                  ; (L -> 1000H, H -> 1001H)

            ; CPU -> Interrupt Mode 2, I = vector table page
            LD   A, VTAB_HI
            LD   I, A
            IM   2

            ; PIO Port A Mode Control Word: Mode 1 = Input
            LD   A, 01001111B
            OUT  (PIOA_CTRL), A

            ; Interrupt vector byte (D0 = 0)
            LD   A, VTAB_LO
            OUT  (PIOA_CTRL), A

            ; Interrupt Control Word: enable INT, no mask word follows
            LD   A, 10000111B
            OUT  (PIOA_CTRL), A

            EI
MAIN:       HALT ; will abort the emulator
            JP   MAIN

; ---------------------------------------------------------
; Interrupt Service Routine (follows main code)
; ---------------------------------------------------------
ISR:        PUSH AF
            PUSH BC
            PUSH DE
            PUSH HL

            IN   A, (PIOA_DATA)
            LD   E, A

            RST  28H
            DB   'PIO INTERRUPT - PORT A = ',0

            LD   A, E
            RST  18H
            DB   068H            ; B2HEX

            RST  18H
            DB   06AH            ; CRLF

            POP  HL
            POP  DE
            POP  BC
            POP  AF
            EI
            RETI
