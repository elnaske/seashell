#pragma once
#include <stdbool.h>
#include <stddef.h>

#include "../options.h"
#include "../sys/redirect.h"

typedef struct Shell Shell;

typedef struct Command {
    char **argv;
    size_t argc;
    Redirect redirects[MAX_REDIRECTS];
    size_t n_redirects;
} Command;

void cmd_add_redirection(Command *cmd, char *file, int fd, int o_flag);

int run_command(Shell *s, Command *cmd, Pipeline *pl, bool is_last);