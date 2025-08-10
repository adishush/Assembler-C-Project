/*
 * SECOND PASS MODULE
 * 
 * This is the THIRD stage of our assembler and the most complex one!
 * Now that we know where all labels are (from first pass), we can finally
 * generate the actual machine code.
 * 
 * What the second pass does:
 * 1. ENCODE INSTRUCTIONS: Convert assembly instructions to binary machine code
 * 2. ENCODE DATA: Convert .data and .string directives to memory values
 * 3. RESOLVE SYMBOLS: Replace label names with their actual addresses
 * 4. HANDLE EXTERNALS: Track references to external symbols
 * 5. GENERATE OUTPUT FILES: Create .ob, .ent, and .ext files
 * 
 * Key concepts:
 * - WORD ENCODING: Each memory word has 12 bits of data + 3 bits ARE (Address/Relocatable/External)
 * - ARE BITS: Tell the loader how to handle each word
 *   - A (4): Absolute - use value as-is
 *   - R (2): Relocatable - add base address when loading
 *   - E (1): External - resolve from other files
 * 
 * Memory layout after second pass:
 * Addresses 100-150: Instructions (machine code)
 * Addresses 151-200: Data (.data and .string values)
 */

#include "assembler.h"  /* Must include this first for basic types */
#include "first_pass.h" /* Need symbol_t and symbol table functions */
#include "second_pass.h"
#include "utils.h"

/* Global memory arrays - these store the final machine code */
word_t instruction_memory[MEMORY_SIZE];  /* Stores encoded instructions */
word_t data_memory[MEMORY_SIZE];         /* Stores data values */
external_ref_t *external_references = NULL; /* List of external symbol uses */

/*
 * SECOND_PASS - Main function for the second pass
 * 
 * Similar structure to first pass, but now we actually generate machine code.
 * We reset IC and DC to their starting values and process the file again,
 * this time encoding everything into our memory arrays.
 */
error_code_t second_pass(const char *filename) {
    FILE *file;
    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    error_code_t result;
    
    /* Reset counters to starting values (same as first pass) */
    IC = INITIAL_IC;  /* Start at 100 */
    DC = INITIAL_DC;  /* Start at 0 */
    
    file = fopen(filename, "r");
    if (!file) {
        return ERROR_FILE_NOT_FOUND;
    }
    
    /* Process each line again, but this time generate machine code */
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        /* Remove newline character */
        line[strcspn(line, "\n")] = 0;
        
        /* Process this line */
        result = process_line_second_pass(line, line_number);
        if (result != SUCCESS) {
            print_error(current_filename, line_number, "Error in second pass");
            error_flag = 1;
        }
    }
    
    fclose(file);
    
    return error_flag ? ERROR_INVALID_SYNTAX : SUCCESS;
}

/*
 * PROCESS_LINE_SECOND_PASS - Process a single line during second pass
 * 
 * Very similar to first pass, but now we encode instead of just validating.
 * We skip the label extraction since labels are already in our symbol table.
 */
error_code_t process_line_second_pass(char *line, int line_number) {
    (void)line_number; /* Unused in current implementation */
    char *line_ptr;
    char *trimmed;
    char *line_copy = NULL;
    int word_count;
    char **words;
    error_code_t result = SUCCESS;
    
    /* Skip empty lines and comments - nothing to encode */
    if (is_empty_line(line) || is_comment_line(line)) {
        return SUCCESS;
    }
    
    /*
     * EXTRACT LABEL (but don't process it)
     * Labels were already added to symbol table in first pass.
     * We extract them here just to clean the line for instruction processing.
     * Example: "MAIN: mov r1, r2" → extract "MAIN", process "mov r1, r2"
     */
    extract_label(line, &line_ptr);  /* We don't need to store the label */
    /*
     * CLEAN UP THE LINE
     * After removing the label, we need to trim whitespace and split into words.
     * This gives us an array of words like ["mov", "r1", "r2"] that we can process.
     */
    
    /* Make a copy since trim_whitespace modifies the string */
    {
        int len = strlen(line_ptr);
        line_copy = malloc(len + 1);
        strcpy(line_copy, line_ptr);
        trimmed = trim_whitespace(line_copy);
    }
    
    /* Split the line into individual words (tokens) */
    {
        char *trimmed_copy = malloc(strlen(trimmed) + 1);
        strcpy(trimmed_copy, trimmed);
        words = split_line(trimmed_copy, &word_count);
        free(trimmed_copy);
    }
    
    /* Nothing to process if line is empty after cleanup */
    if (word_count == 0) {
        free_split_line(words, word_count);
        return SUCCESS;
    }
    
    /*
     * DETERMINE LINE TYPE AND ENCODE APPROPRIATELY
     * 
     * Instructions: Convert to machine code (mov, add, jmp, etc.)
     * Directives: Process data storage (.data, .string, etc.)
     * 
     * At this point we know exactly what each line contains and can
     * generate the appropriate machine code or data values.
     */
    if (is_instruction(words[0])) {
        /* Convert assembly instruction to binary machine code */
        result = encode_instruction_from_parts(words, word_count);
    } else if (is_directive(words[0])) {
        /* Process assembler directives (.data, .string, etc.) */
        result = encode_directive(trimmed);
    }
    /* If it's neither instruction nor directive, it's probably an error or comment */
    
    free_split_line(words, word_count);
    if (line_copy) {
        free(line_copy);
    }
    return result;
}

/*
 * ENCODE_INSTRUCTION - Convert an assembly instruction to machine code
 * 
 * This is the heart of the assembler! It takes an instruction like "mov r1, r2"
 * and converts it to the binary format that the CPU understands.
 * 
 * Instruction encoding format (12 bits):
 * Bits 11-8: Opcode (which instruction: mov=0, add=2, etc.)
 * Bits 7-6:  Source operand type (immediate=0, direct=1, indirect=2, register=3)
 * Bits 5-4:   Destination operand type
 * Bits 3-2:   Source register (if source is register)
 * Bits 1-0:   Destination register (if dest is register)
 * 
 * Additional words may follow for:
 * - Immediate values (#5)
 * - Label addresses (LOOP)
 * - Register numbers (when not both registers)
 */
error_code_t encode_instruction(char *line) {
    char *trimmed = trim_whitespace(line);
    char **parts;
    int part_count;
    instruction_info_t *inst_info;
    opcode_t opcode;
    /* C90: Declare all variables at beginning */
    unsigned int first_word;
    operand_type_t src_type, dest_type;
    
    /*
     * Parse the instruction line into its components.
     * Expected format: [label:] opcode [operand1] [operand2]
     * 
     * For C programming students: This demonstrates string parsing,
     * where we break apart a complex input into manageable pieces.
     */
    parts = split_line(trimmed, &part_count);
    
    if (part_count == 0) {
        free_split_line(parts, part_count);
        return ERROR_INVALID_SYNTAX;
    }
    
    /* Get instruction information */
    inst_info = get_instruction_info(parts[0]);
    if (!inst_info) {
        free_split_line(parts, part_count);
        return ERROR_INVALID_INSTRUCTION;
    }
    
    opcode = inst_info->opcode;
    
    /*
     * ENCODE FIRST WORD (Instruction Word)
     * This word contains the opcode and operand type information
     * 10-bit format: opcode(4) | src(2) | dst(2) | ARE(2)
     */
    first_word = 0;  /* Now assign after declaration */
    first_word |= (opcode << 6);  /* Bits 9-6: opcode (4 bits) */
    
    /* Handle operands based on instruction type */
    if (inst_info->operand_count == 2) {
        /*
         * TWO OPERAND INSTRUCTION (like "mov r1, r2")
         * Need to encode both source and destination operand types
         */
        src_type = get_operand_type(parts[1]);
        dest_type = get_operand_type(parts[2]);
        
        first_word |= (src_type << 4);   /* Bits 5-4: source operand type */
        first_word |= (dest_type << 2);  /* Bits 3-2: destination operand type */
        
        /* Store the first word */
        encode_word(IC++, first_word, ARE_ABSOLUTE);
        
        /* Encode additional words for source and destination operands */
        encode_operand_word(parts[1], src_type);
        encode_operand_word(parts[2], dest_type);
    } else if (inst_info->operand_count == 1) {
        /*
         * ONE OPERAND INSTRUCTION (like "inc r1")
         * Only need to encode destination operand type
         */
        dest_type = get_operand_type(parts[1]);
        first_word |= (dest_type << 2);  /* Bits 3-2: destination operand type */
        
        /* Store the first word */
        encode_word(IC++, first_word, ARE_ABSOLUTE);

        /* Encode the operand's additional word if needed */
        encode_operand_word(parts[1], dest_type);
    } else if (inst_info->operand_count == 0) {
        /* Zero operand instructions (rts, hlt) need no additional encoding */
        encode_word(IC++, first_word, ARE_ABSOLUTE);
    }
    
    free_split_line(parts, part_count);
    return SUCCESS;
}

/*
 * ENCODE_INSTRUCTION_FROM_PARTS - Convert assembly instruction to machine code
 * 
 * This is the heart of the assembler! Here we convert human-readable
 * assembly instructions like "mov r1, r2" into binary machine code.
 * 
 * INSTRUCTION WORD FORMAT (12 bits total):
 * Bits 9-6: Opcode (4 bits) - What operation to perform
 * Bits 5-4: Source operand type (2 bits) - How to interpret source
 * Bits 3-2: Destination operand type (2 bits) - How to interpret destination  
 * Bits 1-0: ARE bits (2 bits) - Address type for loader
 * 
 * OPERAND TYPES:
 * 0 (IMMEDIATE): #5 - literal number
 * 1 (DIRECT): LABEL - memory address
 * 2 (INDIRECT): M1[r2][r7] - matrix with register indexing
 * 3 (REGISTER): r1 - CPU register
 * 
 * WORD GENERATION EXAMPLES:
 * - "stop" → 1 word (just instruction)
 * - "mov r1, r2" → 2 words (instruction + packed registers)
 * - "mov M1[r2][r7], LENGTH" → 4 words (instruction + matrix base + matrix index + destination)
 */
error_code_t encode_instruction_from_parts(char **parts, int part_count) {
    instruction_info_t *inst_info;
    opcode_t opcode;
    /* C90: Declare all variables at beginning of function */
    unsigned int first_word;
    operand_type_t src_type, dest_type;
    int operand_index;
    int value; /* For register-register optimization */
    
    /* Basic validation */
    if (part_count == 0) {
        return ERROR_INVALID_SYNTAX;
    }
    
    /* Look up instruction info (opcode, operand count, etc.) */
    inst_info = get_instruction_info(parts[0]);
    if (!inst_info) {
        return ERROR_INVALID_INSTRUCTION;
    }
    
    opcode = inst_info->opcode;
    
    /*
     * BUILD THE FIRST WORD (Main Instruction Word)
     * This word contains the opcode and operand type information.
     * It's always generated, regardless of operand count.
     */
    first_word = 0;
    first_word |= (opcode << 6);  /* Put opcode in bits 9-6 */
    
    /*
     * HANDLE DIFFERENT OPERAND CONFIGURATIONS
     * Each configuration requires different word generation patterns.
     */
    if (inst_info->operand_count == 0) {
        /* No operands (like "stop") - just encode the instruction word */
        encode_word(IC++, first_word, ARE_ABSOLUTE);
    } else if (inst_info->operand_count == 1) {
        /* Single operand instruction (like "jmp LABEL" or "inc r1") */
        operand_index = 1;
        dest_type = get_operand_type(parts[operand_index]);
        first_word |= (dest_type << 2);  /* Put operand type in bits 3-2 */
        
        /* Generate the instruction word */
        encode_word(IC++, first_word, ARE_ABSOLUTE);
        
        /* Generate additional word for the operand */
        encode_operand_word(parts[operand_index], dest_type);
        
    } else if (inst_info->operand_count == 2) {
        /* Two operand instructions (like "mov r1, r2" or "add M1[r2][r7], LENGTH") */
        src_type = get_operand_type(parts[1]);
        dest_type = get_operand_type(parts[2]);
        
        /* Put both operand types in the instruction word */
        first_word |= (src_type << 4);   /* Source type in bits 5-4 */
        first_word |= (dest_type << 2);  /* Destination type in bits 3-2 */
        
        /* Generate the main instruction word */
        encode_word(IC++, first_word, ARE_ABSOLUTE);
        
        /*
         * REGISTER-REGISTER OPTIMIZATION
         * When both operands are registers, we can pack them into one word
         * instead of generating two separate operand words.
         * This saves memory space: "mov r1, r2" → 2 words instead of 3
         */
        if (src_type == REGISTER && dest_type == REGISTER) {
            int src_reg = get_register_number(parts[1]);
            int dest_reg = get_register_number(parts[2]);
            
            if (src_reg == -1 || dest_reg == -1) {
                return ERROR_INVALID_OPERAND;
            }
            
            /* Pack both register numbers into one word */
            value = (src_reg << 6) | (dest_reg << 3);
            encode_word(IC++, value, ARE_ABSOLUTE);
            
        } else {
            /*
             * NORMAL OPERAND PROCESSING
             * Each operand gets its own word. This handles cases like:
             * - "mov #5, r1" → immediate value + register
             * - "mov LABEL, r2" → address + register  
             * - "mov M1[r2][r7], LENGTH" → matrix addressing + address
             */
            encode_operand_word(parts[1], src_type);
            encode_operand_word(parts[2], dest_type);
        }
    }
    
    return SUCCESS;
}

/*
 * ENCODE_OPERAND_WORD - Generate additional words for operands
 * 
 * Different operand types require different encoding approaches:
 * 
 * IMMEDIATE (#5): Store the literal number
 * DIRECT (LABEL): Store the symbol's address  
 * INDIRECT (M1[r2][r7]): Store matrix base address + index word
 * REGISTER (r1): Store the register number
 * 
 * Each operand word also gets ARE bits to tell the loader how to handle it:
 * - ABSOLUTE: Use value as-is (for immediates, registers)
 * - RELOCATABLE: Add program base address (for internal symbols)
 * - EXTERNAL: Resolve from other files (for external symbols)
 */
error_code_t encode_operand_word(char *operand_str, operand_type_t type) {
    int value;
    symbol_t *symbol;

    switch (type) {
        case IMMEDIATE:
            /* IMMEDIATE VALUES: #5, #-10, etc.
             * Skip the '#' character and convert to integer.
             * These are absolute values that don't need relocation.
             */
            value = string_to_int(operand_str + 1);
            encode_word(IC++, value, ARE_ABSOLUTE);
            break;
            
        case DIRECT:
            /* DIRECT ADDRESSING: LABEL, VARIABLE, etc.
             * Look up the symbol in our symbol table and use its address.
             * May be external (resolved by linker) or internal (add base address).
             */
            {
                char *symbol_name = parse_matrix_operand(operand_str);
                symbol = find_symbol(symbol_name);
            }
            if (symbol) {
                if (symbol->is_external) {
                    /* External symbol - linker will resolve this */
                    encode_word(IC, 0, ARE_EXTERNAL);
                    add_external_reference(operand_str, IC);
                } else {
                    /* Internal symbol - loader will add base address */
                    encode_word(IC, symbol->address, ARE_RELOCATABLE);
                }
                IC++;
            } else {
                print_error(current_filename, 0, "Undefined symbol");
                return ERROR_UNDEFINED_LABEL;
            }
            break;
            
        case INDIRECT:
            /* MATRIX ADDRESSING: M1[r2][r7], ARRAY[r1][r3], etc.
             * This is complex addressing that requires TWO additional words:
             * 1. Base address of the matrix/array
             * 2. Index calculation information
             * This is why matrix instructions generate more words!
             */
            {
                char *symbol_name = parse_matrix_operand(operand_str);
                symbol = find_symbol(symbol_name);
            }
            if (symbol) {
                /* First word: base address of the matrix */
                if (symbol->is_external) {
                    encode_word(IC, 0, ARE_EXTERNAL);
                    add_external_reference(operand_str, IC);
                } else {
                    encode_word(IC, symbol->address, ARE_RELOCATABLE);
                }
                IC++;
                
                /*
                 * Second word: Index register information
                 * Matrix addressing like M1[r2][r7] needs additional computation
                 * at runtime to calculate the final memory address.
                 * This placeholder word would contain index calculation data.
                 */
                if (strstr(operand_str, "[") && strstr(operand_str, "]")) {
                    /* Generate the additional word for matrix indexing */
                    encode_word(IC++, 0, ARE_ABSOLUTE);
                }
            } else {
                print_error(current_filename, 0, "Undefined symbol");
                return ERROR_UNDEFINED_LABEL;
            }
            break;
            
        case REGISTER:
            /* REGISTER ADDRESSING: r0, r1, r2, ..., r7
             * Simply store the register number (0-7).
             * These are absolute values that don't need relocation.
             */
            value = get_register_number(operand_str);
            encode_word(IC++, value, ARE_ABSOLUTE);
            break;
            
        default:
            /* This should never happen if our operand type detection is correct */
            break;
    }
    return SUCCESS;
}

/*
 * ENCODE_DIRECTIVE - Process assembler directives during second pass
 * 
 * Directives don't generate instructions, but some generate data.
 * - .data: Store integer values in data memory
 * - .string: Store string characters in data memory  
 * - .entry: Mark symbols as entry points
 * - .extern: Already handled in first pass
 */
error_code_t encode_directive(char *line) {
    char *trimmed = trim_whitespace(line);
    char **parts;
    int part_count;
    error_code_t result = SUCCESS;
    int i;
    char *str;
    int len;

    /*
     * Parse the directive line.
     * Directives are assembly commands that tell the assembler to do something
     * special, like allocate data storage or define strings.
     * 
     * Common directives:
     * .data - Store numeric values
     * .string - Store text strings  
     * .matrix - Store 2D arrays
     */
    parts = split_line(trimmed, &part_count);
    if (part_count == 0) {
        free_split_line(parts, part_count);
        return SUCCESS;
    }

    if (strcmp(parts[0], ".data") == 0) {
        /* Process each data value - parts[1], parts[2], etc. are the values */
        for (i = 1; i < part_count; i++) {
            /* Each part might contain comma-separated values */
            char *temp_str = malloc(strlen(parts[i]) + 1);
            char *token;
            strcpy(temp_str, parts[i]);
            token = strtok(temp_str, ",");
            while (token) {
                /* Trim whitespace from token */
                while (*token == ' ' || *token == '\t') token++;
                if (strlen(token) > 0) {
                    /*
                     * Convert string representation of number to integer.
                     * Store in data memory with absolute addressing mode.
                     * This is how we convert assembly ".data 5,10,15" 
                     * into actual memory values.
                     */
                    int value = string_to_int(token);
                    data_memory[DC].value = value;
                    data_memory[DC].are = ARE_ABSOLUTE;
                    DC++;  /* Move to next data memory location */
                }
                token = strtok(NULL, ",");
            }
            free(temp_str);
        }
    } else if (strcmp(parts[0], ".string") == 0) {
        if (part_count > 1) {
            /*
             * STRING PROCESSING
             * 
             * Strings in assembly are enclosed in quotes: .string "hello"
             * We need to:
             * 1. Remove the quote marks  
             * 2. Store each character as a separate memory word
             * 3. Add a null terminator (ASCII 0) at the end
             * 
             * This is how C strings work in memory!
             */
            str = parts[1];
            len = strlen(str);
            
            /* Accept both regular quotes (") and Unicode quotes */
            if ((str[0] == '"' && str[len - 1] == '"') || 
                ((unsigned char)str[0] >= 128 && (unsigned char)str[len-1] >= 128)) {
                
                /* Skip the opening and closing quote marks */
                int start_offset = (str[0] == '"') ? 1 : 3;
                int end_offset = (str[len-1] == '"') ? 1 : 3;
                
                /* Store each character in data memory */
                for (i = start_offset; i < len - end_offset; i++) {
                    data_memory[DC].value = str[i];      /* ASCII value of character */
                    data_memory[DC].are = ARE_ABSOLUTE;  /* Direct memory addressing */
                    DC++;
                }
                
                /* Add null terminator (this is how C knows where string ends) */
                data_memory[DC].value = 0; 
                data_memory[DC].are = ARE_ABSOLUTE;
                DC++;
            }
        }
    } else if (strcmp(parts[0], ".entry") == 0) {
        if (part_count > 1) {
            symbol_t *symbol = find_symbol(parts[1]);
            if (symbol) {
                symbol->is_entry = 1;
            } else {
                print_error(current_filename, 0, "Entry symbol not found");
                result = ERROR_UNDEFINED_LABEL;
            }
        }
    } else if (strcmp(parts[0], ".extern") == 0) {
        /* Handled in first pass */
    } else if (strcmp(parts[0], ".mat") == 0) {
        /* Skip the dimensions part [2][2] and process the data values */
        for (i = 2; i < part_count; i++) { /* Data starts after .mat [dims] */
            char *temp_str = malloc(strlen(parts[i]) + 1);
            char *token;
            strcpy(temp_str, parts[i]);
            token = strtok(temp_str, ",");
            while (token) {
                /* Trim whitespace from token */
                while (*token == ' ' || *token == '\t') token++;
                if (strlen(token) > 0) {
                    /*
                     * MATRIX DATA STORAGE
                     * 
                     * Matrices are stored in row-major order in memory.
                     * For a 2x2 matrix: [1,2][3,4] becomes: 1,2,3,4 in memory
                     * This is the standard way C stores 2D arrays.
                     */
                    int value = string_to_int(token);
                    data_memory[DC].value = value;
                    data_memory[DC].are = ARE_ABSOLUTE;
                    DC++;
                }
                token = strtok(NULL, ",");
            }
            free(temp_str);
        }
    }

    free_split_line(parts, part_count);
    return result;
}

/*
 * ADD_EXTERNAL_REFERENCE - Track usage of external symbols
 * 
 * When we reference an external symbol, we need to remember where
 * we used it so the linker can fix it up later.
 * This creates a list of "fixup" locations.
 */
error_code_t add_external_reference(const char *label, int address) {
    external_ref_t *new_ref = malloc(sizeof(external_ref_t));
    if (!new_ref) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    strcpy(new_ref->label, label);
    new_ref->address = address;
    new_ref->next = external_references;
    external_references = new_ref;
    
    return SUCCESS;
}

/*
 * FREE_EXTERNAL_REFERENCES - Clean up external reference list
 */
void free_external_references(void) {
    external_ref_t *current = external_references;
    while (current) {
        external_ref_t *next = current->next;
        free(current);
        current = next;
    }
    external_references = NULL;
}

/*
 * ENCODE_WORD - Store a word in instruction memory
 * 
 * Each word has 12 bits of data plus 3 ARE bits.
 * The ARE bits tell the loader how to handle this word.
 */
error_code_t encode_word(int address, unsigned int value, int are) {
    if (address >= MEMORY_SIZE) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    /* Store in instruction memory array (offset by INITIAL_IC) */
    instruction_memory[address - INITIAL_IC].value = value;
    instruction_memory[address - INITIAL_IC].are = are;
    
    return SUCCESS;
}

/*
 * GENERATE_OBJECT_FILE - Create the .ob output file
 * 
 * The .ob file contains the final machine code in a format that
 * can be loaded and executed. Format:
 * 
 * Line 1: <instruction_count> <data_count>
 * Following lines: <address> <machine_code_in_octal>
 * 
 * Example:
 * 5 3
 * 0100 01234
 * 0101 05432
 * ...
 */
error_code_t generate_object_file(const char *filename) {
    FILE *file;
    char *output_filename;
    int i;  /* C90: Declare loop variable at beginning */
    int final_IC = IC;
    int final_DC = DC;

    /* Create output filename */
    output_filename = create_filename(filename, OB_EXT);
    if (!output_filename) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    file = fopen(output_filename, "w");
    if (!file) {
        free(output_filename);
        return ERROR_FILE_NOT_FOUND;
    }
    
    /*
     * WRITE HEADER LINE
     * First line contains instruction count and data count.
     * This tells the loader how much memory to allocate for each section.
     * Both numbers are encoded in our special base-4 format using letters a-d.
     */
    print_specialbase(file, final_IC - INITIAL_IC);  /* Number of instruction words */
    fprintf(file, " ");
    print_specialbase(file, final_DC);               /* Number of data words */
    fprintf(file, "\n");
    
    /*
     * Write instruction memory
     * Each line: 5-letter address and 5-letter machine code word
     * The word value: 10-bit instruction + 2-bit ARE = 12-bit total
     * But we encode only the 10-bit instruction part as per requirements
     */
    for (i = 0; i < final_IC - INITIAL_IC; i++) {
        int address = INITIAL_IC + i;
        int instruction_10bit = instruction_memory[i].value; /* 10-bit instruction word */
        char binary_str[11]; /* 10 bits + null terminator */
        int bit;
        
        /* Convert 10-bit instruction to binary string */
        for (bit = 9; bit >= 0; bit--) {
            binary_str[9-bit] = (instruction_10bit & (1 << bit)) ? '1' : '0';
        }
        binary_str[10] = '\0';
        
        /* Print: <5-letter address>  <5-letter machine code> */
        fprintf(file, "%s  %s\n", 
                encode_decimal_address_to_letters(address),
                encode_binary10_to_letters(binary_str));
    }
    
    /*
     * Write data memory
     * Data starts immediately after instructions in memory
     */
    for (i = 0; i < final_DC; i++) {
        int address = final_IC + i;
        int data_10bit = data_memory[i].value; /* 10-bit data word */
        char binary_str[11]; /* 10 bits + null terminator */
        int bit;
        
        /* Convert 10-bit data to binary string */
        for (bit = 9; bit >= 0; bit--) {
            binary_str[9-bit] = (data_10bit & (1 << bit)) ? '1' : '0';
        }
        binary_str[10] = '\0';
        
        /* Print: <5-letter address>  <5-letter data value> */
        fprintf(file, "%s  %s\n", 
                encode_decimal_address_to_letters(address),
                encode_binary10_to_letters(binary_str));
    }
    
    fclose(file);
    free(output_filename);
    
    return SUCCESS;
}

/*
 * ENCODE_BINARY10_TO_LETTERS - Convert 10-bit binary string to 5-letter code
 * 
 * Input: 10-bit binary string (e.g., "0000100100")
 * Output: 5-letter encoded string (e.g., "aacba")
 * 
 * Steps:
 * 1. Break into 5 pairs: 00 00 10 01 00
 * 2. Convert pairs to base-4: 0 0 2 1 0
 * 3. Map to letters: a a c b a
 */
char* encode_binary10_to_letters(const char* binary10) {
    static char result[6]; /* 5 letters + null terminator */
    char base4_chars[] = "abcd";
    int i, j;
    
    if (!binary10 || strlen(binary10) != 10) {
        strcpy(result, "aaaaa"); /* Default fallback */
        return result;
    }
    
    /* Process 5 pairs of bits */
    for (i = 0; i < 5; i++) {
        int bit_pair = 0;
        /* Extract 2 bits starting at position i*2 */
        for (j = 0; j < 2; j++) {
            if (binary10[i*2 + j] == '1') {
                bit_pair |= (1 << (1-j)); /* MSB first */
            }
        }
        result[i] = base4_chars[bit_pair];
    }
    result[5] = '\0';
    return result;
}

/*
 * ENCODE_DECIMAL_ADDRESS_TO_LETTERS - Convert decimal address to 5-letter code
 * 
 * Input: decimal address (e.g., 1210)
 * Output: 5-letter encoded address (e.g., "bbacc")
 * 
 * Steps:
 * 1. Convert to base-4: 1210 → 11022
 * 2. Pad to 5 digits if needed
 * 3. Map digits to letters: 1 1 0 2 2 → b b a c c
 */
char* encode_decimal_address_to_letters(int address) {
    static char result[6]; /* 5 letters + null terminator */
    char base4_chars[] = "abcd";
    unsigned int uaddress = (unsigned int)address;
    int i;
    
    /* Convert to base-4, filling from right to left */
    for (i = 4; i >= 0; i--) {
        result[i] = base4_chars[uaddress % 4];
        uaddress /= 4;
    }
    result[5] = '\0';
    return result;
}

/*
 * PRINT_SPECIALBASE - Print a number in base 4 format (legacy function)
 * 
 * This function is kept for backward compatibility but now uses the new encoding functions
 */
void print_specialbase(FILE *file, int value) {
    if (value < 64) {
        /* For small values (counts), use 3 digits - legacy format */
        char* encoded = encode_decimal_address_to_letters(value);
        /* Print only last 3 characters for counts */
        fprintf(file, "%s", encoded + 2);
    } else {
        /* For addresses, use 5 digits */
        char* encoded = encode_decimal_address_to_letters(value);
        fprintf(file, "%s", encoded);
    }
}

/*
 * GENERATE_ENTRIES_FILE - Create the .ent output file
 * 
 * The .ent file lists all entry points (symbols marked with .entry).
 * Other files can reference these symbols.
 * 
 * Format: <symbol_name> <address>
 * Example:
 * MAIN 0100
 * FUNC1 0150
 */
error_code_t generate_entries_file(const char *filename) {
    FILE *file;
    char *output_filename;
    symbol_t *current = symbol_table;
    int has_entries = 0;
    
    /* Check if there are any entry symbols */
    while (current) {
        if (current->is_entry) {
            has_entries = 1;
            break;
        }
        current = current->next;
    }
    
    if (!has_entries) {
        return SUCCESS; /* No entries file needed */
    }
    
    output_filename = create_filename(filename, ENT_EXT);
    if (!output_filename) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    file = fopen(output_filename, "w");
    if (!file) {
        free(output_filename);
        return ERROR_FILE_NOT_FOUND;
    }
    
    /* Write all entry symbols */
    current = symbol_table;
    while (current) {
        if (current->is_entry) {
            fprintf(file, "%s %04d\n", current->name, current->address);
        }
        current = current->next;
    }
    
    fclose(file);
    free(output_filename);
    
    return SUCCESS;
}

/*
 * GENERATE_EXTERNALS_FILE - Create the .ext output file
 */
error_code_t generate_externals_file(const char *filename) {
    FILE *file;
    char *output_filename;
    external_ref_t *current = external_references;
    
    if (!current) {
        return SUCCESS; /* No externals file needed */
    }
    
    output_filename = create_filename(filename, EXT_EXT);
    if (!output_filename) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    file = fopen(output_filename, "w");
    if (!file) {
        free(output_filename);
        return ERROR_FILE_NOT_FOUND;
    }
    
    /* Write all external references */
    while (current) {
        fprintf(file, "%s %04d\n", current->label, current->address);
        current = current->next;
    }
    
    fclose(file);
    free(output_filename);
    
    return SUCCESS;
}

/* END OF FILE - NO MORE FUNCTIONS AFTER THIS */
