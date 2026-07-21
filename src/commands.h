#pragma once
#include "options.h"
#include "redirect.h"
#include <stdbool.h>
#include <stddef.h>
#include <wait.h>

typedef struct Shell Shell;

typedef enum {
    NOT_A_BUILTIN,
    BUILTIN_EXIT,
    BUILTIN_CD,
    BUILTIN_FG,
    BUILTIN_JOBS,
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
} Command;

typedef struct Job {
    Command cmds[MAX_CMDS_PER_JOB];
    size_t cmd_cnt;
    pid_t pgid;
    pid_t last_pid;
    int prev_pipe;
    bool run_in_bg;
} Job;

void free_job(Job *job);

int run_job(Shell *s, Job *job);