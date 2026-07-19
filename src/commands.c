#include "commands.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "parse.h"
#include "redirect.h"
#include "shell.h"
#include "syscall_wrappers.h"

void free_job(Job *job) {
    if (!job) return;
    free(job->cmds[0].argv); // all command argvs share the same allocation, so we only free the first one
    job->cmds[0].argv = NULL;
    job->cmd_cnt = 0;
}

int match_builtin(Command *cmd) {
    char *arg = cmd->argv[0];

    if (strcmp(arg, "exit") == 0)
        return BUILTIN_EXIT;
    if (strcmp(arg, "cd") == 0)
        return BUILTIN_CD;
    if (strcmp(arg, "fg") == 0)
        return BUILTIN_FG;

    return NOT_A_BUILTIN;
}

int builtin_cd(Shell *s, Command *cmd) {
    char *dst = cmd->argc > 1 ? cmd->argv[1] : getenv("HOME");

    if (Chdir(dst) < 0) {
        return -1;
    }

    if (!Getcwd(s->cwd, 100) && errno == ERANGE) {
        memcpy(s->cwd, "../", 4);
    }

    return 0;
}

int builtin_fg(Shell *s, Command *cmd) {
    if (cmd->argc == 1) {
        fprintf(stderr, "TODO: most recent job");
        return -1;
    }

    pid_t pid = strtol(cmd->argv[1], NULL, 10);

    kill(pid, SIGCONT);

    s->fg_pgid = pid;
    Waitpid(pid, NULL, WUNTRACED);
    s->fg_pgid = -1;

    Tcsetpgrp(STDIN_FILENO, s->pgid);

    return 0;
}

int run_builtin(Shell *s, BuiltinKind b, Command *cmd) {
    if (b == NOT_A_BUILTIN)
        return -1;

    SavedFDs saved_fds;
    if (save_fds(&saved_fds) < 0) {
        return -1;
    }
    if (redirect_io(cmd) < 0) {
        restore_fds(&saved_fds);
        return -1;
    }

    int status = 0;

    switch (b) {
    case BUILTIN_EXIT:
        s->running = false;
        break;
    case BUILTIN_CD:
        status = builtin_cd(s, cmd);
        break;
    case BUILTIN_FG:
        status = builtin_fg(s, cmd);
        break;
    default:
        break;
    }

    if (status < 0) {
        restore_fds(&saved_fds);
        return status;
    }

    if (restore_fds(&saved_fds) < 0) {
        return -1;
    }

    return 0;
}

int run_command(Shell *s, Command *cmd, Job *job, bool is_last) {
    if (!cmd || !cmd->argc || !job) return -1;

    (void)s; // TODO: use this for saving status codes

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
            return -1;
        }
        if (redirect_io(cmd) < 0) {
            return -1;
        }

        Execvp(cmd->argv[0], cmd->argv);
    }

    if (job->pgid == 0) {
        job->pgid = pid;
    }

    Setpgid(pid, pid);

    if (close_pipe_read_end(&prev_pipe, pipefd) < 0) {
        return -1;
    }

    job->prev_pipe = prev_pipe;

    return 0;
}

void run_job(Shell *s, Job *job) {
    if (!job || !job->cmd_cnt) return;

    size_t cmds_remaining = job->cmd_cnt;

    for (size_t i = 0; i < job->cmd_cnt; i++) {
        Command cmd = job->cmds[i];
        bool is_last_cmd = (i + 1 >= job->cmd_cnt);

        BuiltinKind b = match_builtin(&cmd);
        if (b != NOT_A_BUILTIN) {
            cmds_remaining--;
            if (run_builtin(s, b, &cmd) < 0) {
                return;
            }
        } else {
            if (run_command(s, &cmd, job, is_last_cmd) < 0) {
                return;
            }
        }

        if (!s->running) {
            return;
        }
    }

    if (!cmds_remaining) {
        return;
    }

    if (!job->run_in_bg) {
        Tcsetpgrp(STDIN_FILENO, job->pgid);

        s->fg_pgid = job->pgid;
        while (cmds_remaining) {
            Waitpid(-job->pgid, NULL, WUNTRACED);
            cmds_remaining--;
        }
        s->fg_pgid = -1;

        Tcsetpgrp(STDIN_FILENO, s->pgid);

    } else {
        printf("%d\n", job->pgid);
    }
}