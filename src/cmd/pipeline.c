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

    int job_status;
    if (WIFSIGNALED(cmd_status)) {
        jt_update_job_state(s, job_id, JOB_STATE_DONE);
        printf("\n");
        job_status = 128 + WTERMSIG(cmd_status);
    } else if (WIFSTOPPED(cmd_status)) {
        jt_update_job_state(s, job_id, JOB_STATE_STOPPED);
        printf("\n[%d] Stopped\n", job_id);
        job_status = 128 + WSTOPSIG(cmd_status);
    } else {
        jt_update_job_state(s, job_id, JOB_STATE_DONE);
        job_status = WEXITSTATUS(cmd_status);
    }

    return job_status;
}

int run_pipeline(Shell *s, Pipeline *pl) {
    if (!pl) return -1;

    size_t cmds_remaining = pl->cmd_cnt;

    for (size_t i = 0; i < pl->cmd_cnt; i++) {
        Command cmd = pl->cmds[i];
        bool is_last_cmd = (i + 1 >= pl->cmd_cnt);
        int exec_status;

        BuiltinKind b = match_builtin(&cmd);
        if (is_builtin(b)) {
            if (pl->run_in_bg) {
                // disallowing for all builtins for now, liable to change if more are added
                fprintf(stderr, "%s: no job control\n", cmd.argv[0]);
                return -1;
            }

            cmds_remaining--;
            if ((exec_status = run_builtin(s, b, &cmd)) != 0) {
                return exec_status;
            }
        } else {
            if (jt_is_full(s)) {
                fprintf(stderr, "Shell error: max number of jobs reached\n");
                return -1;
            }

            if ((exec_status = run_command(&cmd, pl, is_last_cmd)) != 0) {
                return exec_status;
            }
        }

        if (!s->running) {
            return 0;
        }
    }

    int job_status = 0;
    
    if (cmds_remaining) {
        int job_id = jt_add_entry(s, pl);
        if (!job_id_is_valid(s, job_id)) {
            return -1;
        }

        if (!pl->run_in_bg) {
            job_status = await_job(s, job_id, pl->pgid, cmds_remaining);
        } else {
            printf("[%d] %d\n", job_id, pl->pgid);
        }
    }

    return job_status;
}

int init_arg_arena(ArgArena *arena, char **tokens, size_t token_cnt, size_t cmd_line_len, int last_status) {
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