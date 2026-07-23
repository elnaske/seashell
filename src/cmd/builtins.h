#pragma once
#include <stdbool.h>

typedef struct Shell Shell;
typedef struct Command Command;

typedef enum {
    NOT_A_BUILTIN,
    BUILTIN_EXIT,
    BUILTIN_CD,
    BUILTIN_FG,
    BUILTIN_BG,
    BUILTIN_JOBS,
    BUILTIN_KILL,
} BuiltinKind;

bool is_builtin(BuiltinKind b);

int match_builtin(Command *cmd);

int run_builtin(Shell *s, BuiltinKind b, Command *cmd);