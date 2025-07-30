;  Test Example Assembly File
mcro PRINT_MACRO
    mov DATA_VAL, r1
    prn r1
mcroend

mcro CALC_MACRO
    mov #5, r2
    add r2, r1
mcroend


MAIN:   mov #10, r1        ; Load immediate value 10 into r1
        add r1, r2         ; Add r1 and r2, store result in r2
        PRINT_MACRO        ; Use our macro to print a number
        
        ; Test different addressing modes
        mov DATA_VAL, r3   ; Direct addressing - load from DATA_VAL
        mov r3, r4         ; Register to register
        
        ; Matrix operations  
        mov MAT_DATA[r1][r2], r5  ; Matrix addressing
        add MAT_DATA[r0][r1], r5  ; Add matrix element
        
        ; String operations
        lea STR_DATA, r6   ; Load effective address of string
        
        ; Loop example
LOOP:   sub r2, #1         ; Decrement r2
        cmp r2, #0         ; Compare r2 with 0
        bne LOOP           ; Jump back to LOOP if not equal
        CALC_MACRO         ; Use our second macro
        
        ; More instructions
        sub r1, r4         ; Subtract
        inc r7             ; Increment register
        dec r7             ; Decrement register
        not r5             ; Bitwise NOT
        clr r6             ; Clear register
        prn #100           ; Print immediate value
        prn r1             ; Print register content
        red r3             ; Read into register
        
        ; Jump operations
        jmp END_LABEL      ; Unconditional jump
        
MORE:   lea STR_DATA, r7   ; Load effective address
        cmp r3, DATA_VAL   ; Compare register with memory
        bne MORE           ; Branch if not equal
        jsr SUBROUTINE     ; Jump to subroutine
        
SUBROUTINE: mov #42, r0    ; Simple subroutine
        rts                ; Return from subroutine
        
END_LABEL: hlt             ; Halt execution

; Data section
DATA_VAL:    .data 42      ; Single data value with label
LENGTH_VAL:  .data 15      ; Length constant
NUMBERS:     .data 100, -5, 0, 255  ; Multiple data values

; String data
STR_DATA:    .string "Hello Assembly!"  ; String with label
MSG_DATA:    .string "Test Message"     ; Another string

; Matrix data (2x2 matrix)
MAT_DATA:    .mat [2][2] 10, 20, 30, 40

; External and entry declarations
.extern EXTERNAL_SYMBOL
.entry MAIN
.entry DATA_VAL
