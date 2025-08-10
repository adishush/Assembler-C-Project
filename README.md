## Simple Two‑Pass Assembler (C‑Student Friendly)

### What this project is
- **Goal**: Turn a tiny assembly language into machine code.
- **Language**: Plain C, beginner‑friendly (C90 style, standard library only).
- **How it works**: The assembler reads `.as` files and produces output files after two passes over the source.

### The big picture (4 stages)
1. **Macro expansion (.as → .am)**
   - Replaces macro calls with their actual lines.
   - Supports both `mcro ... mcroend` and `macr ... endmacr` styles.
   - Output is an expanded `.am` file with no macro definitions/calls.
2. **First pass (symbol table + sizes)**
   - Scans the `.am` file.
   - Finds labels, calculates how many words each instruction/data needs.
   - Builds a symbol table (label → address). Data labels are adjusted later.
3. **Second pass (encoding)**
   - Scans the `.am` file again.
   - Encodes instructions and data into memory arrays.
   - Resolves labels to addresses.
   - Tracks external references and entry points.
4. **Write outputs**
   - `.ob` object file (encoded words)
   - `.ent` entries (labels you marked as entry)
   - `.ext` externals (labels used from other files)

### Getting started
- Requirements: a C compiler (e.g., `gcc`)
- Build (from project root):
  ```bash
  gcc -std=c11 -Wall -Wextra -O2 -o assembler main.c assembly.c first_pass.c second_pass.c utils.c
  ```
- Run (pass base names, without `.as`):
  ```bash
  ./assembler simple test_example
  ```
  This will look for `simple.as` and `test_example.as`, expand macros to `.am`, then assemble.

### Input format (quick rules)
- One instruction/directive per line. Labels end with a colon:
  ```
  LOOP: mov r1, r2
  ```
- Comments start with `;` and go to the end of the line.
- Registers: `r0` … `r7`.
- Addressing modes:
  - Immediate: `#5`
  - Direct (label): `VALUE`
  - Register: `r3`
  - Matrix-like (indirect): `M1[r2][r7]`
- Common directives: `.data`, `.string`, `.entry`, `.extern`, `.mat` (matrix data).
- Macro definitions:
  ```
  mcro SAVE
    mov r1, TEMP
  mcroend
  ; also supported: macr ... endmacr
  ```

### How to assemble more than one file
- You can pass multiple base names:
  ```bash
  ./assembler simple simple_test math
  ```
- Each file is processed independently. Errors in one do not block others.

### Project structure (what each file does)
- `main.c`: Entry point. Orchestrates the 4 stages for each file.
- `assembler.h`: Shared types, constants, and global variables (IC/DC, error flag, etc.).
- `assembly.h` + `assembly.c`: Macro system.
  - Parse `mcro`/`macr` blocks, store macros, expand calls into the `.am` file.
- `first_pass.h` + `first_pass.c`: First pass (labels + sizes).
  - Builds the symbol table and advances `IC`/`DC` based on instruction/data sizes.
- `second_pass.h` + `second_pass.c`: Second pass (encoding + outputs).
  - Encodes instructions/data into memory arrays and writes `.ob/.ent/.ext`.
- `utils.h` + `utils.c`: Helper functions (string trimming, splitting, instruction info, errors, filename helpers, etc.).
- `*.as`: Sample input assembly files.
- `*.am`: Generated files after macro expansion.
- `*.ob/.ent/.ext`: Outputs from a successful assembly.

### Key ideas explained simply
- IC/DC
  - `IC` (Instruction Counter) starts at 100: where instruction words go.
  - `DC` (Data Counter) starts at 0: where data words go.
  - After first pass, data labels are adjusted by final `IC`, because data comes after code.
- Symbol table (first pass)
  - A linked list of `{name, address, is_external, is_entry, is_data}`.
  - Labels on instructions point to `IC`; labels on data point to `DC` (then adjusted).
- Memory words
  - Each memory word has a 10‑bit value and a 2‑bit A/R/E field (Absolute/Relocatable/External).
- External references
  - If you use a label declared in another file (`.extern`), we record where it’s used to generate `.ext`.

### Instruction set (the basics)
- Supported mnemonics include: `mov, cmp, add, sub, not, clr, lea, inc, dec, jmp, bne, red, prn, jsr, rts, hlt, stop`.
- Operands depend on the instruction (see `instruction_table` in `utils.c`).
- Tips:
  - Many instructions do NOT allow an immediate value as the destination.
  - `cmp` and `prn` are more flexible with immediates.
  - Example decrement: write `sub #1, r2` (not `sub r2, #1`).

### Directives (data and more)
- `.data 1, 2, -7` → stores integers.
- `.string "Hello"` → stores characters plus a terminating 0.
- `.mat [rows][cols] values…` → stores values in row‑major order (e.g., 2x2 becomes 4 values).
- `.extern NAME` → declares a symbol defined in another file.
- `.entry NAME` → marks a symbol to appear in `.ent` (set during second pass).

### Outputs
- `.ob` (object): encoded instruction and data words in a special base format.
- `.ent` (entries): labels marked with `.entry` and their addresses.
- `.ext` (externals): where external labels are referenced.

### Common errors and how to fix them
- “Unknown instruction or directive”
  - Typo in mnemonic or directive. Check spacing and commas.
- “Invalid operand”
  - Wrong number or type of operands. Check allowed modes for the instruction.
- “Duplicate label”
  - The same label is defined more than once (except `.extern` can be declared more than once).
- “Undefined label” (second pass)
  - You used a label that was never defined (and not declared `.extern`).

### How to add or change instructions (optional)
- Edit `instruction_table` in `utils.c` to change allowed operand types.
- If the instruction encodes differently, update `encode_instruction_from_parts` in `second_pass.c`.

### Study tips (why this is good C practice)
- Uses only basic C features you learn early: arrays, structs, pointers, strings, files, and linked lists.
- Clear separation of stages (good modular design).
- Error handling with simple `error_flag` and error codes.

### Try it now
- Build and run on the provided samples:
  ```bash
  gcc -std=c11 -Wall -Wextra -O2 -o assembler main.c assembly.c first_pass.c second_pass.c utils.c
  ./assembler simple simple_test math ps
  ```
- If a file fails the first pass, outputs for that file are not generated (others still run).

### Questions?
- Open the source files. Each function has short comments explaining the “why” in simple terms.
- Start at `main.c` and follow the call chain: macros → first pass → second pass → outputs.