#pragma once

typedef struct Command Command;
typedef struct Shell Shell;

typedef enum {
    NOT_A_BUILTIN,
    BUILTIN_EXIT,
    BUILTIN_CD,
    BUILTIN_FG,
} Builtin;

typedef enum {
    EXEC_OK,
    EXEC_ERR,
} ExecStatus;

int match_builtin(Command *cmd);

int run_builtin(Shell *s, Builtin b, Command *cmd);