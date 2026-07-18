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

void free_cmd(Command *cmd) {
    if (!cmd) return;
    free(cmd->argv);
    cmd->argv = NULL;
    cmd->argc = 0;
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

    int status;

    switch (b) {
    case BUILTIN_EXIT:
        free_cmd(cmd);
        exit(0);
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

void exec_command(Shell *s, Command *cmd) {
    if (!cmd || !cmd->argc) return;

    BuiltinKind b = match_builtin(cmd);

    if (b != NOT_A_BUILTIN) {
        if (run_builtin(s, b, cmd) < 0) {
            return;
        }
    } else {
        pid_t pid = Fork();
        if (pid < 0) {
            return;
        }

        if (pid == 0) {
            Setpgid(0, 0);

            if (redirect_io(cmd) < 0) {
                exit(-1);
            }

            Execvp(cmd->argv[0], cmd->argv);
        }

        Setpgid(pid, pid);

        if (!cmd->run_in_bg) {
            Tcsetpgrp(STDIN_FILENO, pid);

            s->fg_pgid = pid;
            Waitpid(pid, NULL, WUNTRACED);
            s->fg_pgid = -1;

            Tcsetpgrp(STDIN_FILENO, s->pgid);

        } else {
            printf("[%d]", pid);
            for (size_t i = 0; i < cmd->argc; i++) {
                printf(" %s", cmd->argv[i]);
            }
            printf("\n");
        }
    }
}