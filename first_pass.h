/* First pass functions and symbol table */
/* Demonstrates that not all headers need forward declarations */

/* Symbol table entry */
typedef struct symbol {
    char name[MAX_LABEL_LENGTH];
    int address;
    int is_external;
    int is_entry;
    int is_data;
    struct symbol *next;
} symbol_t;

/* Macro table entry */
typedef struct macro {
    char name[MAX_LABEL_LENGTH];
    char **lines;
    int line_count;
    struct macro *next;
} macro_t;

/* Function prototypes */
error_code_t first_pass(const char *filename);
error_code_t process_line_first_pass(char *line, int line_number);
error_code_t add_symbol(const char *name, int address, int is_external, int is_data);
symbol_t *find_symbol(const char *name);
void free_symbol_table(void);
int is_valid_label(const char *label);
int is_instruction(const char *word);
int is_directive(const char *word);
error_code_t process_instruction_first_pass(char **parts, int part_count, const char *label);
error_code_t process_directive_first_pass(char **parts, int part_count, const char *label);
void print_symbol_table(void);

/* Global symbol table */
extern symbol_t *symbol_table;