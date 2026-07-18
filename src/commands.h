#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "redirect.h"
#include "options.h"

typedef struct Shell Shell;

typedef enum {
    NOT_A_BUILTIN,
    BUILTIN_EXIT,
    BUILTIN_CD,
    BUILTIN_FG,
} BuiltinKind;

typedef struct Command {
    char **argv;
    size_t argc;
    Redirect redirects[MAX_REDIRECTS];
    size_t n_redirects;
    bool run_in_bg;
} Command;

void free_cmd(Command *cmd);

int match_builtin(Command *cmd);

int run_builtin(Shell *s, BuiltinKind b, Command *cmd);

void exec_command(Shell *s, Command *cmd);