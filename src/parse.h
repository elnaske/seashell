#include <stddef.h>
#include <stdbool.h>
#pragma once

#define MAX_ARGS 8
#define MAX_REDIRECTS 3

typedef struct {
    char *file;
    int fd;
    int o_flag;
} Redirect;

typedef struct {
    char **argv;
    size_t argc;
    Redirect redirects[MAX_REDIRECTS];
    size_t n_redirects;
    bool run_in_bg;
} Command;

typedef enum {
    PARSE_OK,
    PARSE_ERR_MALLOC,
    PARSE_ERR_FILENAME,
} ParseStatus;

void free_cmd(Command *cmd);

int parse_line(char *line, size_t len, Command *cmd_out);