#include "commands.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../io/redirect.h"
#include "../parser/parse.h"
#include "../sys/syscall_wrappers.h"
#include "../shell.h"

#include "builtins.h"

void free_job(Job *job) {
    if (!job) return;
    free(job->cmds[0].argv); // all command argvs share the same allocation, so we only free the first one
    job->cmds[0].argv = NULL;
    job->cmd_cnt = 0;
}

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
        job_table_update_state(s, job_id, JOB_STATE_DONE);
        printf("\n");
        job_status = 128 + WTERMSIG(cmd_status);
    } else if (WIFSTOPPED(cmd_status)) {
        job_table_update_state(s, job_id, JOB_STATE_STOPPED);
        printf("\n[%d] Stopped\n", job_id);
        job_status = 128 + WSTOPSIG(cmd_status);
    } else {
        job_table_update_state(s, job_id, JOB_STATE_DONE);
        job_status = WEXITSTATUS(cmd_status);
    }

    return job_status;
}


int run_command(Command *cmd, Job *job, bool is_last) {
    if (!cmd || !cmd->argc || !job) return -1;

    int prev_pipe = job->prev_pipe;

    int pipefd[2] = {-1, -1};
    if (!is_last && Pipe(pipefd) < 0) {
        if (prev_pipe > -1) {
            Close(prev_pipe);
        }
        return -1;
    }

    pid_t pid = Fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        Setpgid(0, job->pgid); // pgid = 0 for first command

        if (setup_pipe(prev_pipe, pipefd) < 0) {
            exit(1);
        }
        if (redirect_io(cmd) < 0) {
            exit(1);
        }

        Execvp(cmd->argv[0], cmd->argv);
    }

    if (job->pgid == 0) {
        job->pgid = pid;
    }

    Setpgid(pid, job->pgid);

    if (close_pipe_read_end(&prev_pipe, pipefd) < 0) {
        return -1;
    }

    job->prev_pipe = prev_pipe;
    job->last_pid = pid;

    return 0;
}

int run_job(Shell *s, Job *job) {
    if (!job || !job->cmd_cnt) return -1;

    size_t cmds_remaining = job->cmd_cnt;

    for (size_t i = 0; i < job->cmd_cnt; i++) {
        Command cmd = job->cmds[i];
        bool is_last_cmd = (i + 1 >= job->cmd_cnt);
        int exec_status;

        BuiltinKind b = match_builtin(&cmd);
        if (is_builtin(b)) {
            if (job->run_in_bg) {
                // disallowing for all builtins for now, liable to change if more are added
                fprintf(stderr, "%s: no job control\n", cmd.argv[0]);
                return -1;
            }

            cmds_remaining--;
            if ((exec_status = run_builtin(s, b, &cmd)) != 0) {
                return exec_status;
            }
        } else {
            if (job_table_is_full(s)) {
                fprintf(stderr, "Shell error: max number of jobs reached\n");
                return -1;
            }

            if ((exec_status = run_command(&cmd, job, is_last_cmd)) != 0) {
                return exec_status;
            }
        }

        if (!s->running) {
            return 0;
        }
    }

    int job_status = 0;

    if (cmds_remaining) {
        int job_id = add_job_table_entry(s, job);
        if (job_id < 0) {
            return -1;
        }

        if (!job->run_in_bg) {
            job_status = await_job(s, job_id, job->pgid, cmds_remaining);
        } else {
            printf("[%d] %d\n", job_id, job->pgid);
        }
    }

    return job_status;
}