/* Assembly/macro processing functions */
/* Simple header - no includes needed */

/* Macro definition structure */
typedef struct macro_def {
    char name[MAX_LABEL_LENGTH];    /* Macro name */
    char **content;                 /* Array of content lines */
    int line_count;                 /* Number of lines in macro */
    struct macro_def *next;         /* Next macro in linked list */
} macro_def_t;

/* Function declarations */
error_code_t process_macros(const char *input_filename, const char *output_filename);
error_code_t add_macro(const char *name, char **content, int line_count);
macro_def_t *find_macro(const char *name);
void free_macros(void);
error_code_t expand_macro(FILE *output, const char *macro_name);
int is_macro_definition_start(const char *line);
int is_macro_definition_end(const char *line);
char *extract_macro_name(const char *line);