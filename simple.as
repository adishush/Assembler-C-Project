; Simple test program
macr SAVE_R1
    mov r1, TEMP
endmacr

MAIN: mov #5, r1
      SAVE_R1
      add #10, r1
      prn r1
      hlt

TEMP: .data 0