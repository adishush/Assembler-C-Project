; Working test case for the assembler
; Uses only the instructions that are implemented

START: mov r1, r2
       add #10, r3  
       sub r2, r4
       cmp #0, r1
       
       jmp LOOP
       bne END
       
LOOP:  lea DATA1, r5
       clr r6
       not r7
       inc r0
       dec r1
       jmp END

DATA1: .data 100, 200, 300
STR1:  .string "test"

END:   stop

.entry START
.extern EXTERNAL
