#pragma once
#include "../options.h"
#include "../sys/redirect.h"
#include <stdbool.h>
#include <stddef.h>
#include <wait.h>

typedef struct Shell Shell;

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

typedef struct Pipeline {
    Command cmds[MAX_CMDS_PER_JOB];
    size_t cmd_cnt;
    char *cmd_line;
    pid_t pgid;
    pid_t last_pid;
    int prev_pipe;
    bool run_in_bg;
} Pipeline;

void free_pipeline(Pipeline *pl);

int run_pipeline(Shell *s, Pipeline *pl);

int await_job(Shell *s, int job_id, pid_t pgid, size_t cmd_cnt);