; Test the interrupt using PORT A in input mode and  Port B in mode 3
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
; ---------------------------------------------------------
; I/O port equates
; ---------------------------------------------------------
PIOA_DATA:   EQU 04H
PIOB_DATA:   EQU 05H
PIOA_CTRL:   EQU 06H
PIOB_CTRL:   EQU 07H

; ---------------------------------------------------------
; IM2 vector table page/bytes
; ---------------------------------------------------------
VTAB_HI:     EQU 10H          ; I register = 10H -> table lives at 1000H
VTAB_LO_A:   EQU 00H          ; Port A vector byte (D0 must = 0)
VTAB_LO_B:   EQU 02H          ; Port B vector byte (D0 must = 0, must differ from A)
VTAB_ADDR_A: EQU 1000H        ; Port A pointer table entry (1000H/1001H)
VTAB_ADDR_B: EQU 1002H        ; Port B pointer table entry (1002H/1003H)

; ---------------------------------------------------------
; Main program
; ---------------------------------------------------------
            ORG 0C80H
START:      DI

            ; Build both IM2 pointer table entries in code
            LD   HL, ISR_A
            LD   (VTAB_ADDR_A), HL
            LD   HL, ISR_B
            LD   (VTAB_ADDR_B), HL

            ; CPU -> Interrupt Mode 2, I = vector table page
            LD   A, VTAB_HI
            LD   I, A
            IM   2

            ; --- Port A: Mode 1 (Input) ---
            LD   A, 01001111B     ; 4FH - D7D6=01 input, D1D0=11 mode word ID
            OUT  (PIOA_CTRL), A

            LD   A, VTAB_LO_A     ; Port A interrupt vector byte
            OUT  (PIOA_CTRL), A

            LD   A, 10000111B     ; 87H - INT enable, no mask follows
            OUT  (PIOA_CTRL), A

            ; --- Port B: Mode 3 (Bit Control), bits 0 & 4 as input ---
            LD   A, 11001111B     ; CFH - D7D6=11 mode3, D1D0=11 mode word ID
            OUT  (PIOB_CTRL), A

            LD   A, 00010001B     ; 11H - I/O select mask: bits 0,4 = input
            OUT  (PIOB_CTRL), A

            LD   A, VTAB_LO_B     ; Port B interrupt vector byte
            OUT  (PIOB_CTRL), A

            LD   A, 11110111B     ; F7H - enable, AND, active-High, mask follows
            OUT  (PIOB_CTRL), A

            LD   A, 11101110B     ; EEH - interrupt mask: bits 0,4 monitored
            OUT  (PIOB_CTRL), A

            EI
MAIN:       HALT
            JP   MAIN

; ---------------------------------------------------------
; Interrupt Service Routines (follow main code)
; ---------------------------------------------------------
ISR_A:      PUSH AF
            PUSH BC
            PUSH DE
            PUSH HL

            IN   A, (PIOA_DATA)
            LD   E, A

            RST  28H
            DB   'PIO INT - PORT A = ',0

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

ISR_B:      PUSH AF
            PUSH BC
            PUSH DE
            PUSH HL

            IN   A, (PIOB_DATA)   ; bits 0 & 4 both high
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


