; simple_test.as
mcro TEST_MACRO
    mov DATA_A, r1
    add r1, r2
mcroend

MAIN: mov DATA_A, r1
      add r1, r2
      TEST_MACRO
      mov MATRIX_A[r1][r2], r3
      sub r1, r4
      inc DATA_B
      prn #42
      cmp r1, #0
      bne MAIN
      jmp END_PROG

END_PROG: stop

; Data definitions
DATA_A: .data 10
DATA_B: .data 20, 30, 40
STRING_A: .string "Test String"
MATRIX_A: .mat [2][2] 1, 2, 3, 4

; External and entry declarations
.extern EXTERNAL_FUNC
.entry MAIN
