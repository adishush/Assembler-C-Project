/* Header guard - prevents multiple inclusions (technical requirement) */
#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* File extensions */
#define AS_EXT ".as"     /* Input assembly file */
#define AM_EXT ".am"     /* After macro expansion */
#define OB_EXT ".ob"     /* Object file */
#define ENT_EXT ".ent"   /* Entries file */
#define EXT_EXT ".ext"   /* Externals file */

/* Size limits */
#define MAX_LINE_LENGTH 256
#define MAX_LABEL_LENGTH 32
#define MAX_MACRO_LINES 100      /* Maximum lines in a macro */
#define MAX_FILENAME_LENGTH 256  /* Maximum filename length */
#define MEMORY_SIZE 4096
#define INITIAL_IC 100
#define INITIAL_DC 0
#define MAX_OPERANDS 2           /* Maximum operands per instruction */
#define MAX_FILES 100            /* Maximum number of input files */

/* Operand types */
typedef enum {
    IMMEDIATE = 0,    /* #123 - immediate value */
    DIRECT = 1,       /* LABEL - direct address */
    INDIRECT = 2,     /* *r1 - indirect through register */
    REGISTER = 3      /* r1 - register */
} operand_type_t;

/* ARE (Absolute/Relocatable/External) bits */
typedef enum {
    ARE_ABSOLUTE = 0,     /* A bit - absolute value */
    ARE_EXTERNAL = 1,     /* E bit - external reference */
    ARE_RELOCATABLE = 2   /* R bit - relocatable address */
} are_t;

/* Instruction opcodes */
typedef enum {
    MOV_OP = 0, CMP_OP = 1, ADD_OP = 2, SUB_OP = 3,
    NOT_OP = 4, CLR_OP = 5, LEA_OP = 6, INC_OP = 7,
    DEC_OP = 8, JMP_OP = 9, BNE_OP = 10, RED_OP = 11,
    PRN_OP = 12, JSR_OP = 13, RTS_OP = 14, HLT_OP = 15
} opcode_t;

/* Memory word structure */
typedef struct {
    unsigned int value : 12;  /* 12-bit value */
    unsigned int ARE : 3;     /* 3-bit ARE field */
} word_t;

/* Instruction information structure */
typedef struct {
    char name[MAX_LABEL_LENGTH];   /* Instruction name */
    opcode_t opcode;               /* Numeric opcode */
    int operand_count;             /* Number of operands */
    int valid_src_types[4];        /* Valid source operand types */
    int valid_dest_types[4];       /* Valid destination operand types */
} instruction_info_t;

/* Error codes */
typedef enum {
    SUCCESS = 0,
    ERROR_FILE_NOT_FOUND,
    ERROR_MEMORY_ALLOCATION,
    ERROR_INVALID_SYNTAX,
    ERROR_INVALID_INSTRUCTION,
    ERROR_INVALID_OPERAND,
    ERROR_INVALID_DIRECTIVE,
    ERROR_UNDEFINED_LABEL,
    ERROR_DUPLICATE_LABEL,
    ERROR_LINE_TOO_LONG,
    ERROR_MACRO_NOT_FOUND
} error_code_t;

/* Global variables - declared here, defined in main.c */
extern int IC;  /* Instruction Counter */
extern int DC;  /* Data Counter */
extern int error_flag;
extern char *current_filename;

/* Function declarations */
error_code_t process_assembly_file(const char *filename);
void reset_global_state(void);

#endif /* ASSEMBLER_H */