; Comprehensive test case for the assembler project
; This demonstrates all major features for testing

; Test 1: Basic instructions with different operand types
START:   mov #100, r1      ; immediate to register
         add r1, r2        ; register to register  
         sub LABEL1, r3    ; direct address to register
         cmp #-5, DATA1    ; immediate with direct address

; Test 2: Jump instructions
         jmp LOOP          ; unconditional jump
         bne END           ; conditional jump
         jsr FUNCTION      ; jump to subroutine

; Test 3: Labels and data definitions
LABEL1:  .data 42, -17, 255
DATA1:   .data 1000
ARRAY:   .data 1, 2, 3, 4, 5

; Test 4: String definition
MESSAGE: .string "Hello"

; Test 5: Loop example
LOOP:    lea ARRAY, r4     ; load effective address
         cmp #0, r4        ; compare with zero
         beq END           ; branch if equal
         inc r4            ; increment
         jmp LOOP          ; jump back

; Test 6: Function example  
FUNCTION: prn r1           ; print register
          rts              ; return from subroutine

; Test 7: Different addressing modes
         mov *r1, r2       ; indirect addressing
         add r3, *r4       ; register to indirect

; End of program
END:     stop

; Test 8: External and entry declarations
.extern EXTERNAL_VAR, EXTERNAL_FUNC
.entry START, LOOP, FUNCTION
