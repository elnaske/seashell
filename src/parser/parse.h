#pragma once
#include <stddef.h>

typedef struct Shell Shell;
typedef struct Pipeline Pipeline;

typedef enum {
    PARSE_OK,
    PARSE_ERR_MALLOC,
    PARSE_ERR_FILENAME,
    PARSE_ERR_LEADING_PIPE,
    PARSE_ERR_DANGLING_PIPE,
    PARSE_ERR_AMPERSAND,
} ParseStatus;

int parse_line(Shell *s, char *line, size_t len, Pipeline *pl_out);

void print_syntax_error(int status);