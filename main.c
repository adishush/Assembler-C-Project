/*
 * MAIN MODULE - Entry point for the assembler
 * 
 * This is where everything starts. The main function coordinates all the phases
 * of assembly process and handles multiple input files if given.
 */

#include "assembler.h"
#include "assembly.h"
#include "first_pass.h"
#include "second_pass.h"
#include "utils.h"

/* Global variables - shared across all modules */
int IC = INITIAL_IC;    /* Instruction Counter - starts at 100, tracks where next instruction goes in memory */
int DC = INITIAL_DC;    /* Data Counter - starts at 0, tracks where next data goes in memory */
int error_flag = 0;     /* Flag to track if errors occurred - when this is 1, we don't generate output files */
char *current_filename = NULL;  /* Currently processing filename - using pointer so we can change it dynamically */

/* Function declarations - I put these here so I can define functions in any order I want */
void reset_global_state(void);
error_code_t set_current_filename(const char *filename);
error_code_t process_single_file(const char *base_filename);

/*
 * RESET_GLOBAL_STATE - Clean up everything before processing next file
 * 
 * This is really important because if we process multiple files, we need to
 * start fresh each time. Otherwise data from previous file will mess up current one.
 * I reset counters back to initial values and free all the memory we allocated.
 */
void reset_global_state(void) {
    IC = INITIAL_IC;    /* Reset instruction counter to 100 */
    DC = INITIAL_DC;    /* Reset data counter to 0 */
    error_flag = 0;     /* Clear any previous errors */
    
    /* Free the filename string if we allocated one before */
    if (current_filename) {
        free(current_filename);
        current_filename = NULL;
    }
    
    /* Clean up all the tables we built - these functions handle NULL pointers safely */
    free_macros();              /* Free macro definitions table */
    free_symbol_table();        /* Free symbol table (labels and their addresses) */
    free_external_references(); /* Free list of external references */
}

/*
 * SET_CURRENT_FILENAME - Safely store filename for error messages
 * 
 * I need this because when errors happen, I want to show which file caused the problem.
 * I allocate new memory for the string because the original might get freed or changed.
 * Returns: SUCCESS if everything ok, ERROR_MEMORY_ALLOCATION if malloc fails
 */
error_code_t set_current_filename(const char *filename) {
    /* Free old filename if we had one */
    if (current_filename) {
        free(current_filename);
    }
    
    /* Allocate memory for new filename (+1 for null terminator) */
    current_filename = malloc(strlen(filename) + 1);
    if (!current_filename) {
        return ERROR_MEMORY_ALLOCATION;  /* malloc failed - not enough memory */
    }
    
    /* Copy the string into our allocated memory */
    strcpy(current_filename, filename);
    return SUCCESS;
}

/*
 * PROCESS_SINGLE_FILE - Handle all 4 stages of assembly for one file
 * 
 * This is the main logic that does the actual work. Assembly happens in stages:
 * 1. Macro expansion (.as -> .am) - replace macro calls with actual code
 * 2. First pass (.am file) - build symbol table, count memory needed
 * 3. Second pass (.am file) - generate actual machine code
 * 4. Output generation - create .ob, .ent, .ext files
 * 
 * I use goto cleanup because if any stage fails, I need to free memory before returning.
 * Returns: SUCCESS if everything worked, error code if something failed
 */
error_code_t process_single_file(const char *base_filename) {
    char *input_as_filename = NULL;   /* Will hold "filename.as" */
    char *output_am_filename = NULL;  /* Will hold "filename.am" */
    char *base_name = NULL;           /* Will hold just "filename" for output files */
    error_code_t result = SUCCESS;

    printf("--- Processing file: %s ---\n", base_filename);
    
    /* Create all the filenames we need - adding extensions to base name */
    input_as_filename = create_filename(base_filename, AS_EXT);    /* "name" + ".as" */
    output_am_filename = create_filename(base_filename, AM_EXT);   /* "name" + ".am" */
    base_name = create_filename(base_filename, "");               /* just "name" for .ob/.ent/.ext */

    /* Check if memory allocation worked for all filenames */
    if (!input_as_filename || !output_am_filename || !base_name) {
        fprintf(stderr, "Error: Memory allocation failed for filenames.\n");
        result = ERROR_MEMORY_ALLOCATION;
        goto cleanup;  /* Jump to cleanup section to free whatever we did allocate */
    }

    /* Start fresh - clean up everything from previous file */
    reset_global_state();
    
    /* Set current filename for error messages */
    result = set_current_filename(input_as_filename);
    if (result != SUCCESS) {
        fprintf(stderr, "Error: Failed to set filename.\n");
        goto cleanup;
    }

    /* STAGE 1: MACRO EXPANSION (.as -> .am) */
    /* This stage reads the original .as file and expands any macro calls */
    printf("Stage 1: Expanding macros to '%s'\n", output_am_filename);
    result = process_macros(input_as_filename, output_am_filename);
    if (result != SUCCESS) {
        fprintf(stderr, "Error: Macro expansion failed.\n");
        goto cleanup;
    }

    /* STAGE 2: FIRST PASS (on .am file) */
    /* Now work on the .am file (after macro expansion) */
    /* First pass builds symbol table - finds all labels and calculates their addresses */
    printf("Stage 2: Running first pass on '%s'\n", output_am_filename);
    result = set_current_filename(output_am_filename);  /* Update filename for error messages */
    if (result != SUCCESS) {
        fprintf(stderr, "Error: Failed to update filename.\n");
        goto cleanup;
    }
    
    result = first_pass(output_am_filename);
    if (result != SUCCESS) {
        fprintf(stderr, "Error: First pass failed.\n");
        goto cleanup;
    }

    /* STAGE 3: SECOND PASS (on .am file) */
    /* Second pass generates the actual machine code using symbol table from first pass */
    printf("Stage 3: Running second pass on '%s'\n", output_am_filename);
    result = second_pass(output_am_filename);
    if (result != SUCCESS) {
        fprintf(stderr, "Error: Second pass failed.\n");
        goto cleanup;
    }

    /* STAGE 4: GENERATE OUTPUT FILES */
    /* Only generate output if no errors occurred during assembly */
    if (error_flag == 0) {
        printf("Stage 4: Generating output files for base '%s'\n", base_name);
        generate_object_file(base_name);    /* Creates filename.ob with machine code */
        generate_entries_file(base_name);   /* Creates filename.ent with entry points */
        generate_externals_file(base_name); /* Creates filename.ext with external references */
        printf("--- Successfully processed %s ---\n", base_filename);
    } else {
        /* If there were errors, don't create output files - they would be wrong */
        fprintf(stderr, "Errors were found in '%s'. Output files will not be generated.\n", base_filename);
        result = ERROR_INVALID_SYNTAX;
    }

cleanup:
    /* Always free memory we allocated - even if errors happened */
    free(input_as_filename);
    free(output_am_filename);
    free(base_name);
    return result;
}

int main(int argc, char *argv[]) {
    int i;
    int success_count = 0;    /* Counter for how many files processed successfully */
    int total_files = argc - 1;  /* Total files to process (argv[0] is program name) */

    /* Check if user gave us at least one filename */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] ... (without .as extension)\n", argv[0]);
        return 1;
    }

    /* Process each file the user gave us */
    for (i = 1; i < argc; i++) {
        if (process_single_file(argv[i]) == SUCCESS) {
            success_count++;  /* Count successful files */
        }
        printf("\n"); /* Add blank line between files for cleaner output */
    }
    
    /* Clean up the last filename we stored */
    if (current_filename) {
        free(current_filename);
        current_filename = NULL;
    }

    /* Show summary of what happened */
    printf("Processing complete: %d/%d files successful.\n", success_count, total_files);
    
    /* Return 0 if all files succeeded, 1 if any failed - this is standard Unix convention */
    return (success_count == total_files) ? 0 : 1;
}