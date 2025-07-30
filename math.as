; Math operations test
START: mov #10, r1
       mov #20, r2
       add r2, r1
       sub #5, r1
       prn r1
       hlt

DATA1: .data 100, 200, 300
MSG:   .string "Hello"