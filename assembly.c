/*
 * MACRO PROCESSING MODULE (PRE-ASSEMBLY STAGE)
 * 
 * This is the FIRST stage of our assembler - it handles macro expansion.
 * Think of macros as "find and replace" operations that let programmers
 * define reusable code snippets.
 * 
 * What this module does:
 * 1. Read the original .as file
 * 2. Find macro definitions (macr...endmacr blocks)
 * 3. Store them in a macro table
 * 4. Replace macro calls with their actual content
 * 5. Write the expanded code to a .am file
 * 
 * Example:
 * Input (.as file):
 *   macr SAVE_REGS
 *     mov r1, TEMP1
 *     mov r2, TEMP2
 *   endmacr
 *   
 *   MAIN: SAVE_REGS
 * 
 * Output (.am file):
 *   MAIN: mov r1, TEMP1
 *         mov r2, TEMP2
 */

#include "assembler.h"  /* Must include this first for basic types */
#include "assembly.h"
#include "utils.h"

/* Global macro table - linked list of all defined macros */
macro_def_t *macro_table = NULL;

/*
 * PROCESS_MACROS - Main function for macro expansion
 * 
 * This is like a smart "find and replace" that:
 * 1. Reads the source file line by line
 * 2. When it sees "macr NAME", it starts collecting lines for a macro
 * 3. When it sees "endmacr", it saves the macro definition
 * 4. When it sees a macro name being used, it expands it
 * 5. Everything else gets copied as-is to the output
 * 
 * Parameters:
 * - input_filename: The original .as file (without extension)
 * - output_filename: The .am file to create
 * 
 * Returns: SUCCESS if everything went well, error code otherwise
 */
error_code_t process_macros(const char *input_filename, const char *output_filename) {
    FILE *input;
    FILE *output;
    char line[MAX_LINE_LENGTH];
    int in_macro = 0;
    char macro_name[MAX_LABEL_LENGTH];
    char *macro_content[MAX_MACRO_LINES];  /* Now MAX_MACRO_LINES is defined */
    int macro_line_count = 0;
    int i;  /* Will be used for cleanup if needed */
    char *trimmed;
    macro_def_t *macro;
    
    /* Initialize variables */
    memset(macro_content, 0, sizeof(macro_content));  /* Initialize array to NULL */
    
    /* Open files */
    /* Open input file for reading */
    input = fopen(input_filename, "r");
    if (!input) {
        return ERROR_FILE_NOT_FOUND;
    }
    
    /* Create output file for writing expanded macros */
    output = fopen(output_filename, "w");
    if (!output) {
        fclose(input);
        return ERROR_FILE_NOT_FOUND;
    }
    
    /*
     * MAIN MACRO PROCESSING LOOP
     * 
     * We read through the input file line by line, looking for:
     * 1. Macro definitions (mcro NAME ... mcroend)
     * 2. Macro calls (NAME)
     * 3. Regular assembly code (copy as-is)
     * 
     * This is a two-state machine:
     * - State 0: Normal processing (copy lines, detect macro calls)
     * - State 1: Inside macro definition (collect lines for later use)
     */
    while (fgets(line, sizeof(line), input)) {
        /* Clean up the line */
        line[strcspn(line, "\n")] = 0;
        trimmed = trim_whitespace(line);
        
        /* Copy comments and empty lines without processing */
        if (is_empty_line(trimmed) || (trimmed[0] == ';')) {
            fprintf(output, "%s\n", line);
            continue;
        }
        
        /*
         * MACRO DEFINITION START: "mcro NAME"
         * Switch to macro collection mode.
         */
        if (strncmp(trimmed, "mcro ", 5) == 0 || strncmp(trimmed, "macr ", 5) == 0) {
            in_macro = 1;
            macro_line_count = 0;
            /* Extract macro name (everything after the keyword) */
            if (strncmp(trimmed, "mcro ", 5) == 0) {
                strcpy(macro_name, trimmed + 5);
            } else {
                strcpy(macro_name, trimmed + 5);
            }
            trimmed = trim_whitespace(macro_name);
            strcpy(macro_name, trimmed);
            continue;  /* Don't write macro definition line to output */
        }
        
        /*
         * MACRO DEFINITION END: "mcroend"  
         * Save the collected macro lines and switch back to normal mode.
         */
        if (strcmp(trimmed, "mcroend") == 0 || strcmp(trimmed, "endmacr") == 0) {
            if (in_macro) {
                /* Save the macro we just collected */
                if (macro_line_count > 0) {
                    /* Create a permanent copy of the macro content */
                    char **content_copy = malloc(macro_line_count * sizeof(char*));
                    if (!content_copy) {
                        /* Cleanup on memory allocation failure */
                        for (i = 0; i < macro_line_count; i++) {
                            free(macro_content[i]);
                        }
                        fclose(input);
                        fclose(output);
                        return ERROR_MEMORY_ALLOCATION;
                    }
                    
                    /* Copy all the macro lines */
                    for (i = 0; i < macro_line_count; i++) {
                        content_copy[i] = macro_content[i];
                    }
                    
                    /* Register the macro in our global macro table */
                    add_macro(macro_name, content_copy, macro_line_count);
                }
                in_macro = 0;
                macro_line_count = 0;
            }
            continue;  /* Don't write end marker to output */
        }
        
        /*
         * COLLECT MACRO CONTENT
         * If we're inside a macro definition, collect this line for later use.
         */
        if (in_macro) {
            /* Store this line for the macro */
            if (macro_line_count >= MAX_MACRO_LINES) {
                printf("Error: Macro too long (maximum %d lines)\n", MAX_MACRO_LINES);
                /* Cleanup */
                for (i = 0; i < macro_line_count; i++) {
                    free(macro_content[i]);
                }
                fclose(input);
                fclose(output);
                return ERROR_LINE_TOO_LONG;
            }
            
            /* Allocate memory for this macro line */
            macro_content[macro_line_count] = malloc(strlen(line) + 1);
            if (!macro_content[macro_line_count]) {
                /* Cleanup on memory allocation failure */
                for (i = 0; i < macro_line_count; i++) {
                    free(macro_content[i]);
                }
                fclose(input);
                fclose(output);
                return ERROR_MEMORY_ALLOCATION;
            }
            strcpy(macro_content[macro_line_count], line);
            macro_line_count++;
            
        } else {
            /*
             * NORMAL LINE PROCESSING
             * Check if this line is a macro call. If so, expand it.
             * Otherwise, copy the line to output unchanged.
             */
            macro = NULL;
            {
                char temp_line[MAX_LINE_LENGTH];
                char *p;
                char *semicolon_pos;
                char *colon_pos;
                char macro_candidate[MAX_LABEL_LENGTH];
                int k = 0;
                
                /* Work on a modifiable copy */
                strcpy(temp_line, trimmed);
                
                /* Strip trailing comment if present */
                semicolon_pos = strchr(temp_line, ';');
                if (semicolon_pos) {
                    *semicolon_pos = '\0';
                }
                
                /* Trim whitespace after stripping comments */
                p = trim_whitespace(temp_line);
                
                /* If there's a label (e.g., "LBL: MAC"), skip it */
                colon_pos = strchr(p, ':');
                if (colon_pos) {
                    p = trim_whitespace(colon_pos + 1);
                }
                
                /* Extract first token as candidate macro name */
                while (p[k] && !isspace((unsigned char)p[k]) && k < (MAX_LABEL_LENGTH - 1)) {
                    macro_candidate[k] = p[k];
                    k++;
                }
                macro_candidate[k] = '\0';
                
                if (macro_candidate[0] != '\0') {
                    macro = find_macro(macro_candidate);
                }
            }
            
            if (macro) {
                /* This line is a macro call - replace it with macro content! */
                expand_macro(output, macro->name);
            } else {
                /* Regular assembly line - copy as-is */
                fprintf(output, "%s\n", line);
            }
        }
    }
    
    /* Cleanup any remaining macro content if file ended inside macro definition */
    if (in_macro) {
        printf("Warning: File ended while inside macro definition\n");
        for (i = 0; i < macro_line_count; i++) {
            free(macro_content[i]);
        }
    }
    
    fclose(input);
    fclose(output);
    
    /* Macro processing completed successfully */
    return SUCCESS;
}

/*
 * ADD_MACRO - Add a new macro to our macro table
 * 
 * Creates a new macro entry and adds it to the front of our linked list.
 * This is like adding a new entry to a dictionary.
 * 
 * Parameters:
 * - name: The macro name (like "SAVE_REGS")
 * - content: Array of strings, each string is one line of the macro
 * - line_count: How many lines the macro has
 * 
 * Note: This function takes OWNERSHIP of the content array - don't free it!
 */
error_code_t add_macro(const char *name, char **content, int line_count) {
    /* Allocate memory for new macro structure */
    macro_def_t *new_macro = malloc(sizeof(macro_def_t));
    if (!new_macro) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    /* Fill in the macro information */
    strcpy(new_macro->name, name);      /* Copy the macro name */
    new_macro->content = content;        /* Take ownership of content array */
    new_macro->line_count = line_count;  /* Store number of lines */
    
    /* Add to front of linked list (like a stack) */
    new_macro->next = macro_table;
    macro_table = new_macro;
    
    return SUCCESS;
}

/*
 * FIND_MACRO - Look up a macro by name
 * 
 * Searches through our macro table (linked list) to find a macro
 * with the given name. This is like looking up a word in a dictionary.
 * 
 * Returns: Pointer to macro if found, NULL if not found
 */
macro_def_t *find_macro(const char *name) {
    macro_def_t *current = macro_table;
    
    /* Walk through the linked list */
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;  /* Found it! */
        }
        current = current->next;  /* Move to next macro */
    }
    
    return NULL;  /* Not found */
}

/*
 * FREE_MACROS - Clean up all macro memory
 * 
 * This function frees all the memory we allocated for macros.
 * It's crucial to call this to prevent memory leaks!
 * 
 * For each macro, we need to free:
 * 1. Each line of content
 * 2. The content array itself  
 * 3. The macro structure
 */
void free_macros(void) {
    macro_def_t *current = macro_table;
    macro_def_t *next;
    int i;  /* C90: Variable must be declared at beginning of block */
    
    while (current) {
        next = current->next;  /* Save next pointer before freeing */
        
        /* Free all the content lines */
        for (i = 0; i < current->line_count; i++) {
            free(current->content[i]);
        }
        
        /* Free the content array */
        free(current->content);
        
        /* Free the macro structure itself */
        free(current);
        
        current = next;  /* Move to next macro */
    }
    
    macro_table = NULL;  /* Clear the table pointer */
}

/*
 * IS_MACRO_DEFINITION_START - Check if line starts a macro definition
 * 
 * Looks for lines that start with "macr " (note the space!)
 * Example: "macr SAVE_REGS" returns 1 (true)
 *         "mov r1, r2" returns 0 (false)
 */
int is_macro_definition_start(const char *line) {
    char *trimmed = trim_whitespace((char*)line);
    return strncmp(trimmed, "macr ", 5) == 0;  /* Check first 5 characters */
}

/*
 * IS_MACRO_DEFINITION_END - Check if line ends a macro definition
 * 
 * Looks for lines that are exactly "endmacr"
 * Example: "endmacr" returns 1 (true)
 *         "endmacro" returns 0 (false)
 */
int is_macro_definition_end(const char *line) {
    char *trimmed = trim_whitespace((char*)line);
    return strcmp(trimmed, "endmacr") == 0;
}

/*
 * EXTRACT_MACRO_NAME - Get the macro name from a macro definition line
 * 
 * Takes a line like "macr SAVE_REGS" and returns "SAVE_REGS"
 * 
 * Uses a static buffer, so the returned pointer is valid until
 * the next call to this function.
 */
char *extract_macro_name(const char *line) {
    static char name[MAX_LABEL_LENGTH];  /* Static buffer for the name */
    char *trimmed = trim_whitespace((char*)line);
    
    /* Use sscanf to extract the word after "macr " */
    sscanf(trimmed + 5, "%s", name); /* Skip the "macr " part */
    
    return name;
}

/*
 * EXPAND_MACRO - Write all lines of a macro to the output file
 * 
 * This is where the actual "expansion" happens. When we encounter
 * a macro call in the source code, we replace it with all the lines
 * that were stored in the macro definition.
 * 
 * Example: If SAVE_REGS contains 2 lines, this function writes
 * both lines to the output file.
 */
error_code_t expand_macro(FILE *output, const char *macro_name) {
    macro_def_t *macro;
    int i;  /* C90: Variable must be declared at beginning of function */
    
    /* Look up the macro */
    macro = find_macro(macro_name);
    if (!macro) {
        return ERROR_UNDEFINED_LABEL;  /* Macro not found */
    }
    
    /* Write each line of the macro to the output file */
    for (i = 0; i < macro->line_count; i++) {
        fprintf(output, "%s\n", macro->content[i]);
    }
    
    return SUCCESS;
}

/* END OF FILE - NO MORE FUNCTIONS AFTER THIS */