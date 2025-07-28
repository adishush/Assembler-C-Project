; Simple test case for the assembler
; This tests basic functionality: labels, instructions, and data

START: mov r1, r2
       add #5, r3
       jmp END
       
.data 10, 20, 30

END: stop

.entry START
.extern EXTERNAL_LABEL
