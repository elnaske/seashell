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

// void free_cmd(Command *cmd) {
//     if (!cmd) return;
//     free(cmd->argv);
//     cmd->argv = NULL;
//     cmd->argc = 0;
// }
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
        // free_cmd(cmd);
        // exit(0);
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

int exec_command(Shell *s, Command *cmd, pid_t *pgid, bool is_last, int *prev_p) {
    if (!cmd || !cmd->argc || !pgid || !prev_p) return -1;

    (void)s; // will use this later when saving status codes

    int pipefd[2] = {-1, -1};
    int prev_pipe = *prev_p;

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
        Setpgid(0, *pgid); // pgid = 0 for first command

        if (prev_pipe > -1 && Dup2(prev_pipe, STDIN_FILENO) < 0) {
            Close(prev_pipe);
            return -1;
        }

        if (pipefd[1] > -1 && Dup2(pipefd[1], STDOUT_FILENO) < 0) {
            Close(pipefd[1]);
            return -1;
        }

        if (prev_pipe > -1 && Close(prev_pipe) < 0) {
            return -1;
        }
        if (pipefd[0] > -1) {
            int status = 0;
            status = Close(pipefd[0]);
            status = Close(pipefd[1]);
            if (status < 0) {
                return -1;
            }
        }

        if (redirect_io(cmd) < 0) {
            return -1;
        }

        Execvp(cmd->argv[0], cmd->argv);
    }

    if (*pgid == 0) {
        *pgid = pid;
    }

    Setpgid(pid, pid);

    if (prev_pipe > -1 && Close(prev_pipe) < 0) {
        return -1;
    }

    if (pipefd[1] > -1) {
        if (Close(pipefd[1]) < 0) {
            return -1;
        }
        prev_pipe = pipefd[0];
    } else {
        prev_pipe = -1;
    }

    *prev_p = prev_pipe;

    return 0;
}

void exec_job(Shell *s, Job *job) {
    if (!job || !job->cmd_cnt) return;

    pid_t pgid = 0;
    int prev_pipe = -1;

    for (size_t i = 0; i < job->cmd_cnt; i++) {
        Command cmd = job->cmds[i];

        BuiltinKind b = match_builtin(&cmd);

        if (b != NOT_A_BUILTIN) {
            if (run_builtin(s, b, &cmd) < 0) {
                return;
            }
        } else {
            bool is_last_cmd = i + 1 >= job->cmd_cnt;
            if (exec_command(s, &cmd, &pgid, is_last_cmd, &prev_pipe) < 0) {
                return;
            }
        }

        if (!s->running) {
            return;
        }
    }

    if (prev_pipe > -1) {
        Close(prev_pipe);
    }

    if (!job->run_in_bg) {
        Tcsetpgrp(STDIN_FILENO, pgid);

        s->fg_pgid = pgid;
        while (job->cmd_cnt > 0) {
            Waitpid(-pgid, NULL, WUNTRACED);
            job->cmd_cnt--;
        }
        s->fg_pgid = -1;

        Tcsetpgrp(STDIN_FILENO, s->pgid);

    } else {
        printf("%d\n", pgid);
    }
}