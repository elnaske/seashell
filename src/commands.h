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

typedef enum {
    EXEC_OK,
    EXEC_ERR,
} ExecStatus;

typedef struct Command {
    char **argv;
    size_t argc;
    Redirect redirects[MAX_REDIRECTS];
    size_t n_redirects;
    // bool run_in_bg;
} Command;

typedef struct Job {
    Command cmds[MAX_CMDS_PER_JOB];
    size_t cmd_cnt;
    bool run_in_bg;
} Job;

void free_job(Job *job);

int match_builtin(Command *cmd);

int run_builtin(Shell *s, BuiltinKind b, Command *cmd);

// int exec_command(Shell *s, Command *cmd, pid_t *pgid, bool is_last, int *prev_p);
void exec_job(Shell *s, Job *job);