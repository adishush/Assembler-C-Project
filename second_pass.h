/* Header guard - prevents multiple inclusions (technical requirement) */
#ifndef SECOND_PASS_H
#define SECOND_PASS_H

#include "assembly.h"
#include "first_pass.h"

/* External reference entry */
typedef struct external_ref {
    char label[MAX_LABEL_LENGTH];
    int address;
    struct external_ref *next;
} external_ref_t;

/* Function prototypes */
error_code_t second_pass(const char *filename);
error_code_t process_line_second_pass(char *line, int line_number);
error_code_t encode_instruction(char *line);
error_code_t encode_instruction_from_parts(char **parts, int part_count);
error_code_t encode_directive(char *line);
char* encode_binary10_to_letters(const char* binary10);
char* encode_decimal_address_to_letters(int address);
void print_specialbase(FILE *file, int value);
error_code_t add_external_reference(const char *label, int address);
void free_external_references(void);
error_code_t encode_operand_word(char *operand_str, operand_type_t type);
error_code_t generate_object_file(const char *filename);
error_code_t generate_entries_file(const char *filename);
error_code_t generate_externals_file(const char *filename);
operand_type_t get_operand_type(const char *operand);
int get_register_number(const char *operand);
error_code_t encode_word(int address, unsigned int value, int are);

/* Global variables */
extern word_t instruction_memory[MEMORY_SIZE];
extern word_t data_memory[MEMORY_SIZE];
extern external_ref_t *external_references;

#endif /* SECOND_PASS_H */