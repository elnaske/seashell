#pragma once
#include <stddef.h>

typedef struct Command Command;

typedef enum {
    PARSE_OK,
    PARSE_ERR_MALLOC,
    PARSE_ERR_FILENAME,
} ParseStatus;

void parse_error(int status);

int parse_line(char *line, size_t len, Command *cmd_out);