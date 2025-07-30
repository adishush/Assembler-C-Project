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
 * - WORD ENCODING: Each memory word has 15 bits of data + 3 bits ARE (Address/Relocatable/External)
 * - ARE BITS: Tell the loader how to handle each word
 *   - A (4): Absolute - use value as-is
 *   - R (2): Relocatable - add base address when loading
 *   - E (1): External - resolve from other files
 * 
 * Memory layout after second pass:
 * Addresses 100-150: Instructions (machine code)
 * Addresses 151-200: Data (.data and .string values)
 */

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
    char *trimmed;
    int word_count;
    char **words;
    error_code_t result = SUCCESS;
    /* Removed unused 'label' variable to fix warning */
    
    /* Skip empty lines and comments */
    if (is_empty_line(line) || is_comment_line(line)) {
        return SUCCESS;
    }
    
    /* We don't need to extract labels in second pass since they're already processed */
    trimmed = trim_whitespace(line);
    words = split_line(trimmed, &word_count);
    
    if (word_count == 0) {
        free_split_line(words, word_count);
        return SUCCESS;
    }
    
    /* Encode instructions or directives */
    if (is_instruction(words[0])) {
        result = encode_instruction(line);
    } else if (is_directive(words[0])) {
        result = encode_directive(line);
    }
    
    free_split_line(words, word_count);
    return result;
}

/*
 * ENCODE_INSTRUCTION - Convert an assembly instruction to machine code
 * 
 * This is the heart of the assembler! It takes an instruction like "mov r1, r2"
 * and converts it to the binary format that the CPU understands.
 * 
 * Instruction encoding format (15 bits):
 * Bits 14-11: Opcode (which instruction: mov=0, add=2, etc.)
 * Bits 10-9:  Source operand type (immediate=0, direct=1, indirect=2, register=3)
 * Bits 8-7:   Destination operand type
 * Bits 6-3:   Source register (if source is register)
 * Bits 2-0:   Destination register (if dest is register)
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
    int src_reg, dest_reg;
    int operand_index;
    int value, reg_num;
    symbol_t *symbol;
    
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
     */
    first_word = 0;  /* Now assign after declaration */
    first_word |= (opcode << 11);  /* Bits 14-11: opcode */
    
    /* Handle operands based on instruction type */
    if (inst_info->operand_count == 2) {
        /*
         * TWO OPERAND INSTRUCTION (like "mov r1, r2")
         * Need to encode both source and destination operand types
         */
        src_type = get_operand_type(parts[1]);
        dest_type = get_operand_type(parts[2]);
        
        first_word |= (src_type << 9);   /* Bits 10-9: source operand type */
        first_word |= (dest_type << 7);  /* Bits 8-7: destination operand type */
        
        /*
         * SPECIAL OPTIMIZATION: Both registers in one word
         * If both operands are registers, we can pack them into the first word
         * This saves memory compared to using separate words for each register
         */
        if (src_type == REGISTER && dest_type == REGISTER) {
            src_reg = get_register_number(parts[1]);
            dest_reg = get_register_number(parts[2]);
            first_word |= (src_reg << 6);   /* Bits 8-6: source register */
            first_word |= (dest_reg << 3);  /* Bits 5-3: destination register */
        }
    } else if (inst_info->operand_count == 1) {
        /*
         * ONE OPERAND INSTRUCTION (like "inc r1")
         * Only need to encode destination operand type
         */
        dest_type = get_operand_type(parts[1]);
        first_word |= (dest_type << 7);  /* Bits 8-7: destination operand type */
    }
    /* Zero operand instructions (rts, hlt) need no additional encoding */
    
    /* Store the instruction word in memory */
    encode_word(IC++, first_word, ARE_ABSOLUTE);
    
    /*
     * ENCODE ADDITIONAL WORDS FOR OPERANDS
     * After the instruction word, we may need additional words for:
     * - Immediate values (#123)
     * - Label addresses (LOOP)
     * - Register numbers (when not both registers)
     */
    if (inst_info->operand_count >= 1) {
        /* Handle source operand (if exists) */
        if (inst_info->operand_count == 2) {
            src_type = get_operand_type(parts[1]);
            
            /* Only encode separate word if not using register optimization */
            if (src_type != REGISTER || get_operand_type(parts[2]) != REGISTER) {
                if (src_type == IMMEDIATE) {
                    /*
                     * IMMEDIATE OPERAND (#123)
                     * Store the actual number value
                     */
                    value = string_to_int(parts[1] + 1); /* Skip '#' */
                    encode_word(IC++, value, ARE_ABSOLUTE);
                } else if (src_type == DIRECT) {
                    /*
                     * DIRECT OPERAND (LOOP)
                     * Look up the label and store its address
                     */
                    symbol = find_symbol(parts[1]);
                    if (symbol) {
                        if (symbol->is_external) {
                            /* External symbol - address resolved by linker */
                            encode_word(IC, 0, ARE_EXTERNAL);
                            add_external_reference(parts[1], IC);
                        } else {
                            /* Internal symbol - use its address */
                            encode_word(IC, symbol->address, ARE_RELOCATABLE);
                        }
                        IC++;
                    } else {
                        print_error(current_filename, 0, "Undefined symbol");
                        return ERROR_UNDEFINED_LABEL;
                    }
                } else if (src_type == INDIRECT) {
                    /*
                     * INDIRECT OPERAND (*r1)
                     * Store the register number
                     */
                    reg_num = get_register_number(parts[1] + 1); /* Skip '*' */
                    encode_word(IC++, reg_num, ARE_ABSOLUTE);
                }
            }
        }
        
        /*
         * Handle destination operand
         * Similar to source operand, but this is always the last operand
         */
        operand_index = (inst_info->operand_count == 2) ? 2 : 1;  /* Use pre-declared variable */
        dest_type = get_operand_type(parts[operand_index]);
        
        /* Only encode if not using register optimization */
        if (inst_info->operand_count == 1 || 
            (inst_info->operand_count == 2 && 
             (get_operand_type(parts[1]) != REGISTER || dest_type != REGISTER))) {
            
            if (dest_type == IMMEDIATE) {
                value = string_to_int(parts[operand_index] + 1); /* Skip '#' */
                encode_word(IC++, value, ARE_ABSOLUTE);
            } else if (dest_type == DIRECT) {
                symbol = find_symbol(parts[operand_index]);
                if (symbol) {
                    if (symbol->is_external) {
                        encode_word(IC, 0, ARE_EXTERNAL);
                        add_external_reference(parts[operand_index], IC);
                    } else {
                        encode_word(IC, symbol->address, ARE_RELOCATABLE);
                    }
                    IC++;
                } else {
                    print_error(current_filename, 0, "Undefined symbol");
                    return ERROR_UNDEFINED_LABEL;
                }
            } else if (dest_type == INDIRECT) {
                reg_num = get_register_number(parts[operand_index] + 1); /* Skip '*' */
                encode_word(IC++, reg_num, ARE_ABSOLUTE);
            } else if (dest_type == REGISTER) {
                reg_num = get_register_number(parts[operand_index]);
                encode_word(IC++, reg_num, ARE_ABSOLUTE);
            }
        }
    }
    
    free_split_line(parts, part_count);
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
    int i;  /* C90: Declare loop variable at beginning */
    char *str;
    int len;
    
    parts = split_line(trimmed, &part_count);
    if (part_count == 0) {
        free_split_line(parts, part_count);
        return ERROR_INVALID_SYNTAX;
    }
    
    if (strcmp(parts[0], ".data") == 0) {
        /*
         * .data directive: Store integer values
         * Example: ".data 10, 20, 30" stores three integers in data memory
         */
        for (i = 1; i < part_count; i++) {  /* FIXED: Use pre-declared variable */
            int value = string_to_int(parts[i]);
            data_memory[DC].value = value;
            data_memory[DC].ARE = ARE_ABSOLUTE;
            DC++;
        }
    }
    else if (strcmp(parts[0], ".string") == 0) {
        /*
         * .string directive: Store string characters
         * Example: '.string "hello"' stores 'h','e','l','l','o',0 in data memory
         * Note: We include null terminator for C-style strings
         */
        if (part_count > 1) {
            str = parts[1];
            len = strlen(str);
            
            /* Store each character (skip the quote marks) */
            for (i = 1; i < len - 1; i++) {  /* FIXED: Use pre-declared variable */
                data_memory[DC].value = str[i];
                data_memory[DC].ARE = ARE_ABSOLUTE;
                DC++;
            }
            
            /* Add null terminator */
            data_memory[DC].value = 0;
            data_memory[DC].ARE = ARE_ABSOLUTE;
            DC++;
        }
    }
    else if (strcmp(parts[0], ".entry") == 0) {
        /*
         * .entry directive: Mark symbol as entry point
         * Entry points are symbols that other files can reference
         * Example: ".entry MAIN" makes MAIN visible to other files
         */
        if (part_count > 1) {
            symbol_t *symbol = find_symbol(parts[1]);
            if (symbol) {
                symbol->is_entry = 1;  /* Mark as entry point */
            } else {
                print_error(current_filename, 0, "Entry symbol not found");
                result = ERROR_UNDEFINED_LABEL;
            }
        }
    }
    else if (strcmp(parts[0], ".extern") == 0) {
        /* External symbols already handled in first pass - nothing to do */
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
 * Each word has 15 bits of data plus 3 ARE bits.
 * The ARE bits tell the loader how to handle this word.
 */
error_code_t encode_word(int address, unsigned int value, int are) {
    if (address >= MEMORY_SIZE) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    /* Store in instruction memory array (offset by INITIAL_IC) */
    instruction_memory[address - INITIAL_IC].value = value;
    instruction_memory[address - INITIAL_IC].ARE = are;
    
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
     * Write header line: instruction count and data count
     * This tells the loader how much memory to allocate
     */
    fprintf(file, "%d %d\n", IC - INITIAL_IC, DC);
    
    /*
     * Write instruction memory
     * Each line: address (4 digits) and word value in octal (5 digits)
     * The word value includes the 3 ARE bits in the low 3 bits
     */
    for (i = 0; i < IC - INITIAL_IC; i++) {  /* FIXED: Use pre-declared variable */
        fprintf(file, "%04d %05o\n", INITIAL_IC + i, 
                (instruction_memory[i].value << 3) | instruction_memory[i].ARE);
    }
    
    /*
     * Write data memory
     * Data starts immediately after instructions in memory
     */
    for (i = 0; i < DC; i++) {  /* FIXED: Use pre-declared variable */
        fprintf(file, "%04d %05o\n", IC + i, 
                (data_memory[i].value << 3) | data_memory[i].ARE);
    }
    
    fclose(file);
    free(output_filename);
    
    return SUCCESS;
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