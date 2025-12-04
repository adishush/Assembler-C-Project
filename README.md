# Assembler Project - Systems Programming Course

## Project Overview

This is an assembler written in C that translates assembly language files into machine code. The assembler reads .as files and produces .ob files containing the machine code, along with .ent and .ext files for external symbols.

## Implementation Details

The assembler processes input files in four main stages:

1. **Macro expansion** - handles macro definitions and calls
2. **First pass** - builds symbol table and calculates addresses  
3. **Second pass** - generates machine code using the symbol table
4. **File output** - writes the final object files

I implemented this using several key programming concepts from our course:

### Dynamic Memory Management
I used malloc() and free() to handle variable amounts of data:
- Symbol table entries for labels
- Macro definitions 
- External reference lists

The challenge was making sure every malloc() has a corresponding free() to avoid memory leaks.

### String Processing
Much of assembly language processing involves parsing text:
```c
char *label = extract_label("MAIN: mov r1, r2");  // returns "MAIN"
char **tokens = split_instruction(line);          // splits into parts
```

### Linked Lists for Dynamic Data
Since I don't know ahead of time how many symbols or macros there will be, I used linked lists that can grow as needed.

## File Structure

The project is organized into several modules:

- **main.c** - handles command line input and coordinates the assembly process
- **assembly.c** - processes macros in the first stage
- **first_pass.c** - builds the symbol table 
- **second_pass.c** - generates the actual machine code
- **utils.c** - contains helper functions for parsing and validation

Each module has a corresponding .h file with function declarations and structure definitions.

## Assembly Process

### Macro Processing (assembly.c)
First, the assembler expands any macros. For example:
```assembly
macr SAVE_REGS
    mov r1, TEMP1
    mov r2, TEMP2
endmacr

MAIN: SAVE_REGS
```

becomes:

```assembly
MAIN: mov r1, TEMP1
      mov r2, TEMP2
```

### First Pass (first_pass.c)
The first pass scans through the code to build a symbol table. This is necessary because labels can be referenced before they're defined. For example, we might see `jmp END` before we encounter the `END:` label.

The symbol table stores:
- Label names and their memory addresses
- Whether symbols are external or entry points

### Second Pass (second_pass.c)  
Now that we know where all labels are located, we can generate the actual machine code. This involves:
- Converting assembly instructions to binary
- Replacing label references with actual addresses
- Encoding the results in the required format

## Output Format

The assembler produces three types of output files:

- **.ob files** - contain the machine code in a custom base-4 encoding
- **.ent files** - list symbols that can be accessed by other files  
- **.ext files** - list external symbols referenced in this file

The base-4 encoding uses letters instead of numbers:
- 0 = 'a', 1 = 'b', 2 = 'c', 3 = 'd'

## Error Handling

The assembler includes error checking for:
- Invalid instruction formats
- Undefined labels
- Invalid register names
- Memory addressing errors

When errors are found, the program reports the filename and line number where the error occurred.

## Testing

I included several test files to verify the assembler works correctly:
- simple_test.as - basic functionality
- working_test.as - more complex examples
- comprehensive_test.as - tests all features
- ps.as - matrix operations with macros

## Compilation and Usage

To build the assembler:
```bash
make
```

To assemble a file:
```bash
./assembler filename
```

This will process filename.as and create the appropriate output files.

## Programming Challenges

The main challenges I encountered were:

1. **Forward references** - handling jumps to labels that haven't been defined yet. Solved by using a two-pass approach.

2. **Memory management** - making sure all dynamically allocated memory is properly freed.

3. **Parsing complexity** - assembly language has many different instruction formats that need to be handled correctly.
