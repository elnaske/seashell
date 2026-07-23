#include "pipeline.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../core/shell.h"
#include "../parser/parse.h"
#include "../sys/redirect.h"
#include "../sys/syscall_wrappers.h"

#include "builtins.h"
#include "commands.h"

int await_job(Shell *s, int job_id, pid_t pgid, size_t cmd_cnt) {
    int cmd_status;

    Tcsetpgrp(STDIN_FILENO, pgid);

    s->fg_pgid = pgid;

    size_t cmds_remaining = cmd_cnt;
    while (cmds_remaining) {
        pid_t pid = Waitpid(-pgid, &cmd_status, WUNTRACED);
        if (pid > 0) {
            if (WIFSTOPPED(cmd_status)) {
                break;
            }
            if (WIFEXITED(cmd_status) || WIFSIGNALED(cmd_status)) {
                cmds_remaining--;
            }
        } else if (errno != EINTR) {
            break;
        }
    }

    s->fg_pgid = -1;

    Tcsetpgrp(STDIN_FILENO, s->pgid);

    return jt_update(s, job_id, cmd_status, false);
}

int run_pipeline(Shell *s, Pipeline *pl) {
    if (!pl) return -1;
    if (pl->cmd_cnt == 0) return 0;

    BuiltinKind b = match_builtin(&(pl->cmds[0]));
    if (pl->cmd_cnt == 1 && is_builtin(b) && !pl->run_in_bg) {
        return run_builtin(s, b, &(pl->cmds[0]));
    }

    if (jt_is_full(s)) {
        fprintf(stderr, "Shell error: max number of jobs reached\n");
        return -1;
    }

    for (size_t i = 0; i < pl->cmd_cnt; i++) {
        Command cmd = pl->cmds[i];

        if (!can_run_in_bg(match_builtin(&cmd))) {
            fprintf(stderr, "%s: no job control\n", pl->cmds[0].argv[0]);
            return -1;
        }

        bool is_last_cmd = (i + 1 >= pl->cmd_cnt);
        int cmd_status = run_command(s, &cmd, pl, is_last_cmd);
        if (cmd_status != 0) {
            return cmd_status;
        }
    }

    int job_id = jt_add_entry(s, pl);

    int job_status = 0;
    if (!pl->run_in_bg) {
        job_status = await_job(s, job_id, pl->pgid, pl->cmd_cnt);
    } else {
        printf("[%d] %d\n", job_id, pl->pgid);
    }

    return job_status;
}

int arg_arena_init(ArgArena *arena, char **tokens, size_t token_cnt, size_t cmd_line_len, int last_status) {
    if (!arena) return -1;

    size_t max_argv_len = sizeof(tokens) * (token_cnt + 1);
    size_t exit_code_str_len = sizeof(char) * 4;                // three digits (8-bits) + null terminator
    size_t max_stripped_line_len = sizeof(char) * cmd_line_len; // for printing the job later (len already includes null terminator)

    /*
     * Arena allocation that holds args (pointers into line), a NULL separator, and the previous exit code (last 4 bytes; for expanding $?)
     * i.e.
     * arena: [[argv pointers], NULL, [padding], exit code (str), stripped command line (str)]
     *         |  |   |
     * line:  [..0...0.....0]
     */
    void *mem_arena = malloc(max_argv_len + exit_code_str_len + max_stripped_line_len);
    if (!mem_arena) {
        return -1;
    }

    // add exit code
    char *exit_code_start = (char *)mem_arena + max_argv_len;
    snprintf(exit_code_start, exit_code_str_len, "%d", last_status);

    // copy tokens
    char *cmd_line_start = exit_code_start + exit_code_str_len;
    char *next_token_start = cmd_line_start;
    for (size_t i = 0; i < token_cnt; i++) {
        size_t token_len = strlen(tokens[i]);
        memcpy(next_token_start, tokens[i], token_len);

        if (i + 1 < token_cnt) {
            next_token_start[token_len] = ' ';
        } else {
            next_token_start[token_len] = '\0';
        }

        next_token_start += token_len + 1;
    }

    arena->args = mem_arena;
    arena->last_status = exit_code_start;
    arena->cmd_line = cmd_line_start;

    return 0;
}

void free_pipeline(Pipeline *pl) {
    if (!pl) return;
    free(pl->cmds[0].argv); // arena allocation, so we only free the first cmd
    pl->cmds[0].argv = NULL;
    pl->cmd_cnt = 0;
}