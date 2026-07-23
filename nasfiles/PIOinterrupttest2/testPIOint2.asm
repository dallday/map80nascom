; Test the interrupt using Port B in mode 3
;
; Mode control	11001111	CFH	D7D6=11 selects Mode 3; D1D0=11 identifies mode word
; I/O select mask	00010001	11H	1 = input. Bits 0 and 4 = input, rest = output
; Interrupt vector	00000000	00H	D0=0 required; low byte of IM2 pointer
; Interrupt control	11110111	F7H	D7=enable, D6=AND, D5=active-High, D4=mask follows
; Interrupt mask	11101110	EEH	0 = monitor this bit. Bits 0 & 4 = 0 (monitored), rest = 1 (ignored)
;
; interrupt vectore table is at 0x1000
; it uses the first address for the interrupt call
;
; David Allday July 2026

; With AND + active-High, the PIO only fires the interrupt once both bit 0 and bit 4 of Port B are simultaneously high.
;
;
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

            ; Build the IM2 pointer table entry in code
            LD   HL, ISR
            LD   (VTAB_ADDR), HL

            ; CPU -> Interrupt Mode 2, I = vector table page
            LD   A, VTAB_HI
            LD   I, A
            IM   2

            ; Port B Mode Control Word: Mode 3 (bit/control mode)
            LD   A, 11001111B     ; CFH - D7D6=11 mode3, D1D0=11 mode word ID
            OUT  (PIOB_CTRL), A

            ; I/O select mask: 1=input, 0=output. Bits 0 and 4 = input
            LD   A, 00010001B     ; 11H
            OUT  (PIOB_CTRL), A

            ; Interrupt vector byte (D0 must = 0)
            LD   A, VTAB_LO
            OUT  (PIOB_CTRL), A

            ; Interrupt control word: enable, AND, active-High, mask follows
            LD   A, 11110111B     ; FEH
            OUT  (PIOB_CTRL), A

            ; Interrupt mask word: 0=monitor this bit. Bits 0,4 monitored
            LD   A, 11101110B     ; EEH
            OUT  (PIOB_CTRL), A

            EI
MAIN:       HALT
            JP   MAIN

; ---------------------------------------------------------
; Interrupt Service Routine (follows main code)
; ---------------------------------------------------------
ISR:        PUSH AF
            PUSH BC
            PUSH DE
            PUSH HL

            IN   A, (PIOB_DATA)   ; read Port B (bits 0 & 4 both high)
            LD   E, A

            RST  28H
            DB   'PIO INT - BITS 0&4 HIGH - PORT B = ',0

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
