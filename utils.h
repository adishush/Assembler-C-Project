/* Header guard - prevents multiple inclusions (technical requirement) */
#ifndef UTILS_H
#define UTILS_H

#include "assembler.h"  /* Include assembly.h first to get all type definitions */

/* Function declarations */
char *trim_whitespace(char *str);
char **split_line(char *line, int *count);
void free_split_line(char **parts, int count);
int is_empty_line(const char *line);
int is_comment_line(const char *line);
char *extract_label(char *line, char **line_ptr);
int is_valid_integer(const char *str);
int string_to_int(const char *str);
char *create_filename(const char *base, const char *extension);
void print_error(const char *filename, int line_number, const char *message);
instruction_info_t *get_instruction_info(const char *name);
int is_reserved_word(const char *word);
opcode_t get_opcode(const char *instruction);
int get_instruction_length(const char *instruction, char **operands, int operand_count);
operand_type_t get_operand_type(const char *operand);
int get_register_number(const char *operand);
char *parse_matrix_operand(const char *operand);

#endif /* UTILS_H */