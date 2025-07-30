/*
 * FIRST PASS MODULE
 * 
 * This is the SECOND stage of our assembler. The first pass has two main jobs:
 * 
 * 1. BUILD SYMBOL TABLE: Find all labels and calculate their addresses
 *    - Labels are like "bookmarks" in the code (LOOP:, END:, etc.)
 *    - We need to know where each label points before we can generate machine code
 * 
 * 2. VALIDATE SYNTAX: Check that everything looks correct
 *    - Valid instructions and operands
 *    - Proper directive usage
 *    - No duplicate labels
 * 
 * Why do we need a "first pass"?
 * Consider this code:
 *   jmp END
 *   ...
 *   END: halt
 * 
 * When we see "jmp END", we don't know where END is yet! We need to scan
 * the whole file first to build a table of all labels and their addresses.
 * 
 * The first pass DOESN'T generate machine code - it just figures out
 * where everything should go in memory.
 */

#include "first_pass.h"
#include "utils.h"

/* Global symbol table - our "dictionary" of labels and their addresses */
symbol_t *symbol_table = NULL;

/*
 * FIRST_PASS - Main function for the first pass
 */
error_code_t first_pass(const char *filename) {
    FILE *file;
    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    error_code_t result = SUCCESS;
    symbol_t *current;  /* ADD THIS LINE - missing variable declaration */
    
    printf("DEBUG: first_pass() called with filename: %s\n", filename);
    
    file = fopen(filename, "r");
    if (!file) {
        printf("DEBUG: Could not open file: %s\n", filename);
        print_error(filename, 0, "Could not open file");
        return ERROR_FILE_NOT_FOUND;
    }
    
    printf("DEBUG: Successfully opened file for first pass\n");
    
    /* Process each line of the file */
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        printf("DEBUG: Processing line %d: %s", line_number, line);
        
        /* Remove newline character */
        line[strcspn(line, "\n")] = 0;
        
        result = process_line_first_pass(line, line_number);
        if (result != SUCCESS) {
            printf("DEBUG: Error processing line %d\n", line_number);
            print_error(filename, line_number, "Error in first pass");
            /* Don't return immediately - continue processing to find all errors */
        }
    }
    
    fclose(file);
    
    /*
     * CRUCIAL STEP: Update data symbol addresses
     * 
     * During the pass, we assigned data symbols addresses starting from 0.
     * But in the final memory layout, data comes AFTER all instructions.
     * So we need to add the final IC value to all data symbol addresses.
     * 
     * Example:
     * - Instructions occupy addresses 100-150 (IC goes from 100 to 151)
     * - Data symbol was at address 5, now becomes 151 + 5 = 156
     */
    current = symbol_table;  /* NOW this variable is declared */
    while (current) {
        if (current->is_data) {
            current->address += IC;  /* Move data after instructions */
        }
        current = current->next;
    }
    
    return error_flag ? ERROR_INVALID_SYNTAX : SUCCESS;
}

/*
 * PROCESS_LINE_FIRST_PASS - Process a single line during first pass
 * 
 * This function is the "brain" of the first pass. For each line, it:
 * 1. Skips empty lines and comments
 * 2. Extracts any label present
 * 3. Determines if it's an instruction or directive
 * 4. Calls the appropriate processing function
 * 
 * Labels are tricky - they can appear on lines with instructions:
 * "LOOP: mov r1, r2"  <- LOOP is a label, mov r1, r2 is an instruction
 * "DATA: .data 5, 10" <- DATA is a label, .data 5, 10 is a directive
 */
error_code_t process_line_first_pass(char *line, int line_number) {
    char *label;
    char *line_ptr = NULL;
    char *trimmed;
    int word_count;
    char **words;
    error_code_t result = SUCCESS;

    if (is_empty_line(line) || is_comment_line(line)) {
        return SUCCESS;
    }

    label = extract_label(line, &line_ptr);

    trimmed = trim_whitespace(line_ptr);
    words = split_line(trimmed, &word_count);

    if (word_count == 0) {
        if (label) {
            if (add_symbol(label, IC, 0, 0) != SUCCESS) {
                result = ERROR_DUPLICATE_LABEL;
            }
        }
        free_split_line(words, word_count);
        return result;
    }

    if (is_instruction(words[0])) {
        result = process_instruction_first_pass(words, word_count, label);
    } else if (is_directive(words[0])) {
        result = process_directive_first_pass(words, word_count, label);
    } else {
        print_error(current_filename, line_number, "Unknown instruction or directive");
        result = ERROR_INVALID_INSTRUCTION;
    }

    free_split_line(words, word_count);
    return result;
}

/*
 * PROCESS_INSTRUCTION_FIRST_PASS - Handle an instruction line during first pass
 * 
 * For instructions, we need to:
 * 1. Add any label to symbol table (pointing to current IC)
 * 2. Validate the instruction syntax
 * 3. Calculate how much memory this instruction will take
 * 4. Advance IC by that amount
 * 
 * We DON'T generate machine code yet - that's for the second pass!
 * We just need to know "this instruction will take X words of memory"
 * so we can assign correct addresses to future labels.
 */
error_code_t process_instruction_first_pass(char **parts, int part_count, const char *label) {
    instruction_info_t *inst_info;
    int instruction_length;

    if (label && strlen(label) > 0) {
        if (add_symbol(label, IC, 0, 0) != SUCCESS) {
            return ERROR_DUPLICATE_LABEL;
        }
    }

    inst_info = get_instruction_info(parts[0]);
    if (!inst_info) {
        return ERROR_INVALID_INSTRUCTION;
    }

    instruction_length = get_instruction_length(parts[0], &parts[1], part_count - 1);
    if (instruction_length < 0) {
        return ERROR_INVALID_OPERAND;
    }

    IC += instruction_length;

    return SUCCESS;
}

/*
 * PROCESS_DIRECTIVE_FIRST_PASS - Handle a directive line during first pass
 * 
 * Directives are assembler commands that don't generate machine instructions.
 * Instead, they:
 * - Reserve memory for data (.data, .string)
 * - Mark symbols as external (.extern) 
 * - Mark symbols as entry points (.entry)
 * 
 * For data directives, we need to calculate how much memory they'll need
 * and advance DC (Data Counter) accordingly.
 */
error_code_t process_directive_first_pass(char **parts, int part_count, const char *label) {
    error_code_t result = SUCCESS;
    int i;

    if (strcmp(parts[0], ".data") == 0) {
        if (label && strlen(label) > 0) {
            add_symbol(label, DC, 0, 1);
        }
        for (i = 1; i < part_count; i++) {
            DC++;
        }
    } else if (strcmp(parts[0], ".string") == 0) {
        if (label && strlen(label) > 0) {
            add_symbol(label, DC, 0, 1);
        }
        if (part_count > 1) {
            DC += strlen(parts[1]) - 1;
        }
    } else if (strcmp(parts[0], ".entry") == 0) {
        /* No action needed in first pass */
    } else if (strcmp(parts[0], ".extern") == 0) {
        if (part_count > 1) {
            add_symbol(parts[1], 0, 1, 0);
        }
    } else {
        result = ERROR_INVALID_DIRECTIVE;
    }

    return result;
}

/*
 * ADD_SYMBOL - Add a new symbol to the symbol table
 * 
 * The symbol table is our "phone book" of labels and addresses.
 * Each symbol has:
 * - name: The label name (like "LOOP", "DATA")
 * - address: Where in memory this symbol points
 * - is_external: Is this symbol defined in another file?
 * - is_entry: Should this symbol be visible to other files?
 * - is_data: Does this symbol point to data (vs. instruction)?
 * 
 * We use a linked list to store symbols - each new symbol goes at the front.
 */
error_code_t add_symbol(const char *name, int address, int is_external, int is_data) {
    symbol_t *existing;
    symbol_t *new_symbol;  /* C90: Declare all variables at beginning */
    
    /* Check for duplicate symbols (except externals can be redeclared) */
    existing = find_symbol(name);
    if (existing && !is_external) {
        return ERROR_DUPLICATE_LABEL;
    }
    
    /* Allocate memory for new symbol */
    new_symbol = malloc(sizeof(symbol_t));
    if (!new_symbol) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    /* Fill in symbol information */
    strcpy(new_symbol->name, name);
    new_symbol->address = address;
    new_symbol->is_external = is_external;
    new_symbol->is_entry = 0;      /* Entry status set in second pass */
    new_symbol->is_data = is_data;
    
    /* Add to front of linked list */
    new_symbol->next = symbol_table;
    symbol_table = new_symbol;
    
    return SUCCESS;
}

/*
 * FIND_SYMBOL - Look up a symbol in the symbol table
 * 
 * Simple linear search through our linked list of symbols.
 * Returns pointer to symbol if found, NULL if not found.
 */
symbol_t *find_symbol(const char *name) {
    symbol_t *current = symbol_table;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;  /* Found it! */
        }
        current = current->next;
    }
    return NULL;  /* Not found */
}

/*
 * FREE_SYMBOL_TABLE - Clean up symbol table memory
 */
void free_symbol_table(void) {
    symbol_t *current;
    symbol_t *next;  /* C90: Declare all variables at beginning */
    
    current = symbol_table;
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }
    symbol_table = NULL;
}

/*
 * IS_VALID_LABEL - Check if a string is a valid label name
 * 
 * Valid labels must:
 * - Start with a letter
 * - Contain only letters and numbers
 * - Be less than MAX_LABEL_LENGTH characters
 * - Not be a reserved word (instruction name, register name, etc.)
 */
int is_valid_label(const char *label) {
    int i;  /* C90: Declare loop variable at beginning */
    
    /* Basic validation */
    if (!label || strlen(label) == 0 || strlen(label) >= MAX_LABEL_LENGTH) {
        return 0;
    }
    
    /* Must start with letter */
    if (!isalpha(label[0])) {
        return 0;
    }
    
    /* All characters must be alphanumeric */
    for (i = 1; label[i]; i++) {  /* FIXED: Use pre-declared variable */
        if (!isalnum(label[i])) {
            return 0;
        }
    }
    
    /* Must not be a reserved word */
    return !is_reserved_word(label);
}

/*
 * IS_INSTRUCTION - Check if a word is a valid instruction name
 * 
 * Simply checks if we can find the word in our instruction table.
 */
int is_instruction(const char *word) {
    return get_instruction_info(word) != NULL;
}

/*
 * IS_DIRECTIVE - Check if a word is a valid directive
 * 
 * Directives are the assembler commands that start with dot (.)
 */
int is_directive(const char *word) {
    return (strcmp(word, ".data") == 0 ||
            strcmp(word, ".string") == 0 ||
            strcmp(word, ".entry") == 0 ||
            strcmp(word, ".extern") == 0);
}

/*
 * PRINT_SYMBOL_TABLE - Debug function to display all symbols
 * 
 * Useful for debugging - shows all symbols we found and their properties.
 * Not used in normal assembly process, but helpful for understanding
 * what the assembler is doing.
 */
void print_symbol_table(void) {
    symbol_t *current = symbol_table;
    printf("\nSymbol Table:\n");
    printf("Name\t\tAddress\tExternal\tEntry\tData\n");
    printf("----\t\t-------\t--------\t-----\t----\n");
    while (current) {
        printf("%-15s\t%d\t%s\t\t%s\t%s\n",
               current->name,
               current->address,
               current->is_external ? "Yes" : "No",
               current->is_entry ? "Yes" : "No",
               current->is_data ? "Yes" : "No");
        current = current->next;
    }
    printf("\n");
}