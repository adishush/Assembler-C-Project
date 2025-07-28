/*
 * MAIN MODULE - Entry point for the assembler
 */

#include "assembler.h"
#include "assembly.h"
#include "first_pass.h"
#include "second_pass.h"
#include "utils.h"

/* Global variables - shared across all modules */
int IC = INITIAL_IC;    /* Instruction Counter - tracks current instruction address */
int DC = INITIAL_DC;    /* Data Counter - tracks current data address */
int error_flag = 0;     /* Flag to track if errors occurred */
char *current_filename = NULL;  /* Currently processing filename - FIXED: Use pointer, not array */

/*
 * RESET_GLOBAL_STATE - Reset all global variables for processing a new file
 */
void reset_global_state(void) {
    IC = INITIAL_IC;
    DC = INITIAL_DC;
    error_flag = 0;
    if (current_filename) {
        free(current_filename);
        current_filename = NULL;
    }
    free_macros();
    free_symbol_table();
    free_external_references();
}

/*
 * SET_CURRENT_FILENAME - Safely set the current filename for error reporting
 */
void set_current_filename(const char *filename) {
    if (current_filename) {
        free(current_filename);
    }
    current_filename = malloc(strlen(filename) + 1);
    if (current_filename) {
        strcpy(current_filename, filename);
    }
}

int main(int argc, char *argv[]) {
    int i;
    char *input_as_filename = NULL;
    char *output_am_filename = NULL;
    char *base_name = NULL;
    error_code_t result;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] ... (without .as extension)\n", argv[0]);
        return 1;
    }

    for (i = 1; i < argc; i++) {
        printf("--- Processing file: %s ---\n", argv[i]);
        
        /* Create all necessary filenames from the base argument */
        input_as_filename = create_filename(argv[i], AS_EXT);
        output_am_filename = create_filename(argv[i], AM_EXT);
        base_name = create_filename(argv[i], ""); /* For .ob, .ent, .ext files */

        if (!input_as_filename || !output_am_filename || !base_name) {
            fprintf(stderr, "Error: Memory allocation failed for filenames.\n");
            free(input_as_filename);
            free(output_am_filename);
            free(base_name);
            continue;
        }

        /* Reset all counters and tables for the new file */
        reset_global_state();
        set_current_filename(input_as_filename);

        /* STAGE 1: MACRO EXPANSION (.as -> .am) */
        printf("Stage 1: Expanding macros to '%s'\n", output_am_filename);
        result = process_macros(input_as_filename, output_am_filename);
        if (result != SUCCESS) {
            fprintf(stderr, "Error: Macro expansion failed.\n");
            goto cleanup; /* Skip to the next file */
        }

        /* STAGE 2: FIRST PASS (on .am file) */
        printf("Stage 2: Running first pass on '%s'\n", output_am_filename);
        set_current_filename(output_am_filename); /* Update for error messages */
        result = first_pass(output_am_filename);  /* <-- CRITICAL FIX: Use .am file */
        if (result != SUCCESS) {
            fprintf(stderr, "Error: First pass failed.\n");
            goto cleanup;
        }

        /* STAGE 3: SECOND PASS (on .am file) */
        printf("Stage 3: Running second pass on '%s'\n", output_am_filename);
        result = second_pass(output_am_filename);  /* <-- CRITICAL FIX: Use .am file */
        if (result != SUCCESS) {
            fprintf(stderr, "Error: Second pass failed.\n");
            goto cleanup;
        }

        /* STAGE 4: GENERATE OUTPUT FILES */
        if (error_flag == 0) {
            printf("Stage 4: Generating output files for base '%s'\n", base_name);
            generate_object_file(base_name);
            generate_entries_file(base_name);
            generate_externals_file(base_name);
            printf("--- Successfully processed %s ---\n\n", argv[i]);
        } else {
            fprintf(stderr, "Errors were found in '%s'. Output files will not be generated.\n", argv[i]);
        }

    cleanup:
        free(input_as_filename);
        free(output_am_filename);
        free(base_name);
    }
    
    /* Final cleanup of last filename */
    if (current_filename) {
        free(current_filename);
        current_filename = NULL;
    }

    return 0;
}