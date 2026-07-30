#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <wait.h>

#include "../options.h"
#include "commands.h"

typedef struct Shell Shell;

typedef struct ArgArena {
    char **args;
    char *last_status;
    char *line_tokenized;
    char *cmd_line;
} ArgArena;


typedef struct Pipeline {
    Command cmds[MAX_CMDS_PER_JOB];
    size_t cmd_cnt;
    char *cmd_line;
    pid_t pgid;
    pid_t last_pid;
    int prev_pipe;
    bool run_in_bg;
} Pipeline;

int run_pipeline(Shell *s, Pipeline *pl);

int await_job(Shell *s, int job_id, pid_t pgid, size_t cmd_cnt);

int arg_arena_init(ArgArena *arena, size_t cmd_line_len, int last_status);

void free_arg_arena(ArgArena *arena);

void free_pipeline(Pipeline *pl);