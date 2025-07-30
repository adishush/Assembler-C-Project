; External symbols test
.extern printf
.entry MAIN

MAIN: mov #42, r1
      jsr printf
      hlt