/*
 * UTILITY FUNCTIONS MODULE
 * 
 * This file contains helper functions that are used throughout the assembler.
 * Think of this as a "toolbox" with various useful functions that multiple
 * parts of our assembler need to use.
 * 
 * Key concepts:
 * - String manipulation (trimming, splitting)
 * - Instruction validation and information lookup
 * - File name creation
 * - Error reporting
 */

#include "utils.h"

/* 
 * INSTRUCTION LOOKUP TABLE
 * 
 * This table defines all the instructions our assembler supports.
 * Each entry contains:
 * - name: The instruction mnemonic (like "mov", "add")
 * - opcode: The numeric code for this instruction (0-15)
 * - operand_count: How many operands this instruction expects (0, 1, or 2)
 * - valid_src_types: Which operand types are valid for source operand
 * - valid_dest_types: Which operand types are valid for destination operand
 * 
 * Operand types: 0=immediate (#5), 1=direct (LABEL), 2=indirect (*r1), 3=register (r1)
 * Array format: {immediate, direct, indirect, register}
 * 1 = allowed, 0 = not allowed
 */
instruction_info_t instruction_table[] = {
    /* name   opcode   count  source_types     dest_types */
    {"mov", MOV_OP, 2, {0,1,1,1}, {0,1,1,1}},  /* mov can't have immediate dest */
    {"cmp", CMP_OP, 2, {1,1,1,1}, {1,1,1,1}},  /* cmp allows all types */
    {"add", ADD_OP, 2, {0,1,1,1}, {0,1,1,1}},  /* add can't have immediate dest */
    {"sub", SUB_OP, 2, {0,1,1,1}, {0,1,1,1}},  /* sub can't have immediate dest */
    {"not", NOT_OP, 1, {0,0,0,0}, {0,1,1,1}},  /* not: one operand, no immediate dest */
    {"clr", CLR_OP, 1, {0,0,0,0}, {0,1,1,1}},  /* clr: one operand, no immediate dest */
    {"lea", LEA_OP, 2, {0,1,0,0}, {0,1,1,1}},  /* lea: source must be direct only */
    {"inc", INC_OP, 1, {0,0,0,0}, {0,1,1,1}},  /* inc: one operand, no immediate dest */
    {"dec", DEC_OP, 1, {0,0,0,0}, {0,1,1,1}},  /* dec: one operand, no immediate dest */
    {"jmp", JMP_OP, 1, {0,0,0,0}, {0,1,0,1}},  /* jmp: direct or register only */
    {"bne", BNE_OP, 1, {0,0,0,0}, {0,1,0,1}},  /* bne: direct or register only */
    {"red", RED_OP, 1, {0,0,0,0}, {0,1,1,1}},  /* red: one operand, no immediate dest */
    {"prn", PRN_OP, 1, {0,0,0,0}, {1,1,1,1}},  /* prn: allows all dest types */
    {"jsr", JSR_OP, 1, {0,0,0,0}, {0,1,0,1}},  /* jsr: direct or register only */
    {"rts", RTS_OP, 0, {0,0,0,0}, {0,0,0,0}},  /* rts: no operands */
    {"hlt", HLT_OP, 0, {0,0,0,0}, {0,0,0,0}},  /* hlt: no operands */
    {"", -1, 0, {0,0,0,0}, {0,0,0,0}}           /* End marker - signals end of table */
};

 /*
 * GET_OPERAND_TYPE - Determine the type of an operand
 * 
 * Operand types in our assembly language:
 * - IMMEDIATE (0): #123 - immediate value
 * - DIRECT (1): LABEL - direct address 
 * - INDIRECT (2): *r1 - indirect through register
 * - REGISTER (3): r1 - register
 * 
 * This function looks at the first character(s) to determine type.
 */
operand_type_t get_operand_type(const char *operand) {
    if (!operand) return -1;
    
    if (operand[0] == '#') {
        return IMMEDIATE;    /* #123 */
    } else if (operand[0] == '*') {
        return INDIRECT;     /* *r1 */
    } else if (operand[0] == 'r' && isdigit(operand[1]) && operand[2] == '\0') {
        return REGISTER;     /* r1, r2, etc. */
    } else {
        return DIRECT;       /* LABEL */
    }
}

/*
 * GET_REGISTER_NUMBER - Extract register number from register operand
 * 
 * Takes strings like "r1", "r7" and returns the number (1, 7).
 * Returns -1 if not a valid register.
 * Our CPU has registers r0 through r7.
 */
int get_register_number(const char *operand) {
    int reg_num;
    
    if (!operand || operand[0] != 'r') return -1;
    
    reg_num = operand[1] - '0';  /* Convert character to number */
    return (reg_num >= 0 && reg_num <= 7) ? reg_num : -1;
}

/*
 * TRIM_WHITESPACE - Remove leading and trailing spaces from a string
 * 
 * This is super useful for cleaning up input lines that might have
 * extra spaces at the beginning or end.
 * 
 * Example: "  hello world  " becomes "hello world"
 * 
 * How it works:
 * 1. Move pointer forward past leading spaces
 * 2. Find the end of the string
 * 3. Move backwards from end, removing trailing spaces
 * 4. Add null terminator at the new end
 */
char *trim_whitespace(char *str) {
    char *end;
    
    /* Skip leading whitespace - keep moving pointer forward while we see spaces */
    while (isspace((unsigned char)*str)) str++;
    
    /* If string is all spaces, return empty string */
    if (*str == 0) 
        return str;
    
    /* Find the end of the string */
    end = str + strlen(str) - 1;
    
    /* Move backwards from end, removing trailing spaces */
    while (end > str && isspace((unsigned char)*end)) end--;
    
    /* Add null terminator after the last non-space character */
    end[1] = '\0';
    
    return str;
}

/*
 * SPLIT_LINE - Break a line into individual words/tokens
 * 
 * This function is like the "split" function in Python - it takes a line
 * and breaks it up into separate words, handling commas, spaces, and tabs.
 * 
 * Example: "mov r1, r2" becomes ["mov", "r1", "r2"]
 * 
 * Parameters:
 * - line: The input string to split (gets modified!)
 * - count: Pointer to store how many parts we found
 * 
 * Returns: Array of strings (char**), or NULL if memory allocation fails
 * 
 * IMPORTANT: Caller must free the returned array using free_split_line()
 */
char **split_line(char *line, int *count) {
    char **parts = NULL;    /* Array to store the split parts */
    char *token;            /* Current token from strtok */
    int capacity = 10;      /* Initial capacity - will grow if needed */
    char **temp;            /* C90: Declare at beginning for realloc */
    
    *count = 0;  /* Start with zero parts found */
    
    /* Allocate initial array for storing parts */
    parts = malloc(capacity * sizeof(char*));
    if (!parts) {
        return NULL;  /* Memory allocation failed */
    }
    
    /* Use strtok to break line into tokens, separated by space, tab, or comma */
    token = strtok(line, " \t,");
    while (token != NULL) {
        /* If we've filled our array, make it bigger (dynamic array growth) */
        if (*count >= capacity) {
            capacity *= 2;  /* Double the size */
            temp = realloc(parts, capacity * sizeof(char*));  /* Use pre-declared variable */
            if (!temp) {
                free_split_line(parts, *count);  /* Clean up on failure */
                return NULL;
            }
            parts = temp;
        }
        
        /* Allocate memory for this token and copy it */
        parts[*count] = malloc(strlen(token) + 1);
        if (!parts[*count]) {
            free_split_line(parts, *count);
            return NULL;
        }
        strcpy(parts[*count], token);
        (*count)++;
        
        /* Get next token */
        token = strtok(NULL, " \t,");
    }
    
    return parts;
}

/*
 * FREE_SPLIT_LINE - Clean up memory allocated by split_line()
 * 
 * This function frees all the memory that split_line() allocated.
 * Always call this after you're done with the result from split_line()!
 * 
 * C programming rule: For every malloc(), there must be a free()!
 */
void free_split_line(char **parts, int count) {
    int i;  /* C90: Declare loop variable at beginning */
    
    if (!parts) return;  /* Nothing to free */
    
    /* Free each individual string */
    for (i = 0; i < count; i++) {  /* FIXED: Use pre-declared variable */
        free(parts[i]);
    }
    
    /* Free the array itself */
    free(parts);
}

/*
 * IS_EMPTY_LINE - Check if a line contains only whitespace
 * 
 * Returns 1 (true) if the line is empty or contains only spaces/tabs/newlines
 * Returns 0 (false) if the line has actual content
 * 
 * This helps us skip blank lines when processing assembly files.
 */
int is_empty_line(const char *line) {
    if (!line) return 1;  /* NULL pointer = empty */
    
    /* Check each character - if we find a non-space, it's not empty */
    while (*line) {
        if (!isspace((unsigned char)*line)) {
            return 0;  /* Found non-whitespace = not empty */
        }
        line++;
    }
    return 1;  /* All characters were whitespace = empty */
}

/*
 * IS_COMMENT_LINE - Check if a line is a comment
 * 
 * In assembly language, comments start with semicolon (;)
 * This function checks if a line (after trimming spaces) starts with ;
 */
int is_comment_line(const char *line) {
    char *trimmed = trim_whitespace((char*)line);
    return (trimmed[0] == ';');  /* Return true if first character is ; */
}

/*
 * EXTRACT_LABEL - Extract label from a line and return pointer to the rest.
 *
 * Labels in assembly end with a colon (:)
 * Example: "LOOP: mov r1, r2"
 * - Extracts "LOOP" as the label.
 * - Sets *line_ptr to point to " mov r1, r2".
 *
 * Returns: Pointer to static buffer containing the label, or NULL if no label.
 *
 * IMPORTANT: Uses a static buffer for the label, so its value is only valid
 *            until the next call to this function.
 */
char *extract_label(char *line, char **line_ptr) {
    static char label[MAX_LABEL_LENGTH];
    char *colon_pos;
    int label_len;

    colon_pos = strchr(line, ':');

    if (!colon_pos) {
        *line_ptr = line; /* No label, rest of the line is the original line */
        return NULL;
    }

    label_len = colon_pos - line;
    if (label_len >= MAX_LABEL_LENGTH) {
        *line_ptr = line; /* Treat as if no label was found */
        return NULL; /* Label too long */
    }

    strncpy(label, line, label_len);
    label[label_len] = '\0';

    *line_ptr = colon_pos + 1; /* Point to the part after the colon */

    return trim_whitespace(label);
}

/*
 * IS_VALID_INTEGER - Check if a string represents a valid integer
 * 
 * Accepts: "123", "-456", "+789"
 * Rejects: "12a", "abc", "", "1.5"
 * 
 * This is used to validate immediate operands like #123
 */
int is_valid_integer(const char *str) {
    if (!str || *str == '\0') return 0;  /* Empty or NULL string */
    
    /* Skip optional + or - sign */
    if (*str == '+' || *str == '-') str++;
    
    /* Check that all remaining characters are digits */
    while (*str) {
        if (!isdigit(*str)) return 0;  /* Found non-digit */
        str++;
    }
    
    return 1;  /* All characters were digits */
}

/*
 * STRING_TO_INT - Convert string to integer
 * 
 * Simple wrapper around atoi() for consistency
 * In a real assembler, you might want more error checking here
 */
int string_to_int(const char *str) {
    return atoi(str);
}

/*
 * CREATE_FILENAME - Create a new filename with different extension
 * 
 * Takes a base filename and replaces its extension with a new one
 * Example: create_filename("program.as", ".am") returns "program.am"
 * 
 * If no extension exists, just appends the new extension
 * Example: create_filename("program", ".am") returns "program.am"
 * 
 * Returns: Newly allocated string with the filename, or NULL on error
 * IMPORTANT: Caller must free() the returned string!
 */
char *create_filename(const char *base, const char *extension) {
    char *filename;
    char *dot_pos;
    int base_len;
    
    /* Find the last dot in the filename (start of current extension) */
    dot_pos = strrchr(base, '.');
    if (dot_pos) {
        /* Extension found - use everything before the dot */
        base_len = dot_pos - base;
    } else {
        /* No extension - use entire filename */
        base_len = strlen(base);
    }
    
    /* Allocate memory for new filename */
    filename = malloc(base_len + strlen(extension) + 1);
    if (!filename) {
        return NULL;  /* Memory allocation failed */
    }
    
    /* Copy base name (without extension) */
    strncpy(filename, base, base_len);
    filename[base_len] = '\0';
    
    /* Append new extension */
    strcat(filename, extension);
    
    return filename;
}

/*
 * PRINT_ERROR - Display error message with file and line information
 * 
 * Standardized error reporting function used throughout the assembler
 * Makes it easy to show users exactly where problems occurred
 */
void print_error(const char *filename, int line_number, const char *message) {
    if (line_number > 0) {
        /* Include line number if provided */
        fprintf(stderr, "Error in file %s, line %d: %s\n", filename, line_number, message);
    } else {
        /* General file error without specific line */
        fprintf(stderr, "Error in file %s: %s\n", filename, message);
    }
}

/*
 * GET_INSTRUCTION_INFO - Look up information about an instruction
 * 
 * Searches the instruction_table for the given instruction name
 * Returns pointer to the instruction info, or NULL if not found
 * 
 * This is how we validate that "mov" is a real instruction and
 * find out how many operands it needs, what types are allowed, etc.
 */
instruction_info_t *get_instruction_info(const char *name) {
    int i;  /* C90: Declare loop variable at beginning */
    
    /* Linear search through instruction table */
    for (i = 0; instruction_table[i].name[0] != '\0'; i++) {  /* Use name sentinel to end table */
        if (strcmp(instruction_table[i].name, name) == 0) {
            return &instruction_table[i];  /* Found it! */
        }
    }
    return NULL;  /* Not found */
}

/*
 * IS_RESERVED_WORD - Check if a word is reserved (can't be used as label)
 * 
 * Reserved words include:
 * - All instruction names (mov, add, etc.)
 * - All directive names (.data, .string, etc.)
 * - All register names (r0, r1, ..., r7)
 * - Macro keywords (macr, endmacr)
 * 
 * This prevents users from creating labels like "mov:" or "r1:"
 */
int is_reserved_word(const char *word) {
    /* Check if word is an instruction name */
    if (get_instruction_info(word)) {
        return 1;  /* Found in instruction table = reserved */
    }
    
    /* Check if word is a directive */
    if (strcmp(word, ".data") == 0 || strcmp(word, ".string") == 0 ||
        strcmp(word, ".entry") == 0 || strcmp(word, ".extern") == 0) {
        return 1;
    }
    
    /* Check if word is a register name (r0 through r7) */
    if (word[0] == 'r' && strlen(word) == 2 && 
        word[1] >= '0' && word[1] <= '7') {
        return 1;
    }
    
    /* Check macro-related keywords */
    if (strcmp(word, "macr") == 0 || strcmp(word, "endmacr") == 0) {
        return 1;
    }
    
    return 0;  /* Not reserved */
}

/*
 * GET_OPCODE - Get the numeric opcode for an instruction
 * 
 * Simple wrapper that looks up instruction and returns its opcode
 * Returns -1 if instruction not found
 */
opcode_t get_opcode(const char *instruction) {
    instruction_info_t *info = get_instruction_info(instruction);
    return info ? info->opcode : (opcode_t)-1;
}

/*
 * GET_INSTRUCTION_LENGTH - Calculate how many memory words an instruction needs
 * 
 * This is CRUCIAL for the first pass - we need to know how much memory
 * each instruction will take up so we can assign correct addresses to labels
 * 
 * Length calculation:
 * - Base instruction word: 1 word (always)
 * - Each operand: usually 1 additional word
 * - SPECIAL CASE: Two registers can share one word (saves space)
 * 
 * Examples:
 * - "mov r1, r2" = 2 words (1 base + 1 shared register word)
 * - "mov #5, r1" = 3 words (1 base + 1 immediate + 1 register)
 * - "mov LABEL, r1" = 3 words (1 base + 1 address + 1 register)
 */
int get_instruction_length(const char *instruction, char **operands, int operand_count) {
    instruction_info_t *info;
    int length;
    int i;  /* C90: Declare loop variable at beginning */
    operand_type_t type;
    
    /* Look up instruction information */
    info = get_instruction_info(instruction);
    if (!info) {
        return -1;  /* Unknown instruction */
    }
    
    /* Verify operand count matches what instruction expects */
    if (operand_count != info->operand_count) {
        return -1;  /* Wrong number of operands */
    }
    
    length = 1; /* Start with base instruction word */
    
    /* Check each operand and add to length */
    for (i = 0; i < operand_count; i++) {  /* FIXED: Use pre-declared variable */
        type = get_operand_type(operands[i]);  /* NOW WORKS: function is defined above */
        
        /* Validate that this operand type is allowed in this position */
        if (i == 0) { /* Source operand (first operand) */
            if (!info->valid_src_types[type]) {
                return -1;  /* Invalid source operand type */
            }
        } else { /* Destination operand (second operand) */
            if (!info->valid_dest_types[type]) {
                return -1;  /* Invalid destination operand type */
            }
        }
        
        /* 
         * SPECIAL OPTIMIZATION: Two registers can share one word
         * If we have 2 operands and both are registers, they share one word
         * Otherwise, each operand gets its own word
         */
        if (operand_count == 2 && i == 0 && type == REGISTER && 
            get_operand_type(operands[1]) == REGISTER) {
            /* This is the first of two registers - they'll share one word */
            /* Don't increment length here, the shared word will be counted once */
        } else if (type != REGISTER || operand_count == 1) {
            /* This operand needs its own word */
            length++;
        }
    }
    
    return length;
}