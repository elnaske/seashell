#pragma once

typedef struct {
    char **argv;
    size_t argc;
    char *stdin_redirect;
    char *stdout_redirect;
    char *stderr_redirect;
    bool stdout_append;
    bool stderr_append;
    bool run_in_bg;
} Command;

void free_cmd(Command *cmd);

int parse_line(char *line, size_t len, Command *cmd_out);