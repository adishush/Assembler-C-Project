## Two-Pass Assembler in C (Student-Friendly README)

We are a team of two students who built a small assembler in C. Our program reads a simple assembly language and converts it into machine-like code. We wrote it with learning in mind, so the code tries to be clear and safe, and the README explains things in plain English.

### What this program does
- Input: one or more assembly source files with the `.as` extension (without giving the extension on the command line).
- Stage 1 (macro expansion): expands `mcro ... mcroend` macros and writes a `.am` file.
- Stage 2 (first pass): scans the `.am` file to build a symbol table (labels → addresses) and validate syntax.
- Stage 3 (second pass): uses the symbol table to encode instructions/data into memory words.
- Output: creates up to three files per input program:
  - `.ob` — object code in a special base-4 letter format
  - `.ent` — entry symbols and their addresses (if there are any)
  - `.ext` — external symbol references and where they were used (if there are any)

Pipeline summary: `.as → .am → (first pass) → (second pass) → .ob / .ent / .ext`

### How to build
We target C90 and try to keep warnings clean.

```sh
cd /workspace
gcc -std=c90 -Wall -Wextra -pedantic main.c assembly.c first_pass.c second_pass.c utils.c -o assembler
```

### How to run
- Pass base filenames without the `.as` suffix.
- You can pass multiple files and the assembler will process them one by one.

```sh
./assembler sample test_example
```

This will look for `sample.as` and `test_example.as`, expand macros to `.am`, run two passes, and then write output files like `sample.ob`, `sample.ent`, `sample.ext` (the `.ent`/`.ext` are created only if needed).

If you run without arguments, the program prints usage help.

### The assembly language we support (quick guide)
- Labels: `MAIN:` on a line marks a label.
- Instructions: `mov`, `cmp`, `add`, `sub`, `not`, `clr`, `lea`, `inc`, `dec`, `jmp`, `bne`, `red`, `prn`, `jsr`, `rts`, `hlt` (also `stop` as an alias for `hlt`).
- Addressing modes we recognize:
  - Immediate: `#5`
  - Direct label: `VALUE`
  - Register: `r0`..`r7`
  - Indirect (including matrix-style): `*r1` or `M1[r2][r7]` (we parse the base name and treat it as a direct symbol; indices imply register usage)
- Directives we handle: `.data`, `.string`, `.entry`, `.extern`
- Macros: use `mcro NAME` on a line, and `mcroend` to end the macro block, then call it by writing `NAME` on a line.

Example macro usage:
```asm
mcro PRINT_MACRO
    mov DATA_VAL, r1
    prn r1
mcroend

MAIN:   mov #10, r1
        PRINT_MACRO
```

Note: Some sample files in this repo also show `macr ... endmacr`. Our macro expander recognizes the `mcro ... mcroend` form.

### What the output files look like
- `.ob` header line has two counts: number of instruction words and number of data words. We print counts and values using a “base-4 with letters” format: `a=0`, `b=1`, `c=2`, `d=3`. Addresses are shown as 5 letters, words as 5 letters derived from the 10-bit payload.
- `.ent` lists `name address` lines for every symbol declared as `.entry`.
- `.ext` lists `label address` lines for every place that referenced an external symbol.

### Project structure (what each file does)
- `main.c`: orchestrates everything. For each input:
  - builds filenames (`.as`, `.am`, and base),
  - runs macro expansion → first pass → second pass,
  - if there were no errors, writes `.ob/.ent/.ext`.
  - Resets global state between files.
- `assembly.c` + `assembly.h`: macro processor — builds a macro table, expands calls, writes the `.am` file.
- `first_pass.c` + `first_pass.h`: symbol table builder — parses lines, validates instructions/directives, computes how many memory words each instruction or data directive needs, updates `IC` and `DC`, and finally shifts data-label addresses by the final `IC`.
- `second_pass.c` + `second_pass.h`: encoder — converts instructions and data to memory words, resolves labels, tracks external references, and generates `.ob/.ent/.ext`.
- `utils.c` + `utils.h`: helpers — tokenization, trimming, filename creation, error reporting, instruction table and validation, operand type detection, register parsing, special base-4 encoding utilities.
- `assembler.h`: common types and constants (opcodes, word format, limits, error codes) and globals (`IC`, `DC`, `error_flag`, `current_filename`).
- Sample inputs: `sample.as`, `simple.as`, `test_example.as`, `comprehensive_test.as`, etc.

### Key design ideas (what we practiced and learned)
- Two-pass assembler design:
  - Pass 1 builds the symbol table and calculates sizes (no machine code yet).
  - Pass 2 encodes instructions/data now that label addresses are known.
- Memory model:
  - Instruction Counter `IC` starts at 100; Data Counter `DC` starts at 0.
  - After pass 1, data-label addresses are shifted by final `IC` so data follows instructions.
  - We store encoded words in two arrays: `instruction_memory[]` and `data_memory[]`.
- Word encoding and bitfields:
  - We use a compact `word_t` with bitfields: a 10-bit value plus 2 ARE bits (Absolute/Relocatable/External).
  - We output words/addresses in a readable base-4 letter encoding (`a`..`d`).
- Data structures:
  - Linked lists for the macro table, the symbol table, and the list of external references.
  - Dynamic arrays for tokenization using `malloc`/`realloc`.
- Parsing and validation:
  - Splitting lines with `strtok`, trimming whitespace, classifying operands, checking valid addressing modes per instruction.
- Error handling and UX:
  - Clear `error_code_t` enum and an `error_flag` that prevents generating outputs if we found issues.
  - `current_filename` is updated so error messages include file and line numbers.
  - We reset global state between files and free all allocated memory (`free_macros`, `free_symbol_table`, `free_external_references`).

### Limits we set (on purpose, to keep things simple)
- Max line length: 256; max label length: 32; max macro lines: 100.
- Registers are `r0`..`r7`.
- We focus on a small instruction set and a few addressing modes.

### Tips for trying the samples
- Good starting files: `simple.as`, `test_example.as`, `sample.as`.
- If you want to see `.ent` and `.ext`, include `.entry` and `.extern` directives in your source.
- If there’s a syntax error, we will report it, skip output file creation, and continue to the next input file so you can see multiple errors in one run.

### Teamwork
- We split the work between macro + first pass and encoder + output generation, and then pair-reviewed each other’s code. Both of us worked across files to keep the style consistent and to understand the whole pipeline end-to-end.

### License
For course/learning use.