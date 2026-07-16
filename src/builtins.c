#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "parse.h"
#include "redirect.h"

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

int builtin_cd(Command *cmd) {
    char *dst = cmd->argc > 1 ? cmd->argv[1] : getenv("HOME");

    if (Chdir(dst) < 0) {
        return EXEC_ERR;
    }

    // TODO: fix this w/ shell struct
    if (!Getcwd(cwd, 100) && errno == ERANGE) {
        memcpy(cwd, "../", 4);
    }

    return EXEC_OK;
}

int builtin_fg(Command *cmd) {
    if (cmd->argc == 1) {
        printf("TODO: most recent job");
        return EXEC_OK;
    }

    pid_t pid = strtol(cmd->argv[1], NULL, 10);

    kill(pid, SIGCONT);

    // TODO: fix this w/ shell struct
    fg_pgid = pid;
    Waitpid(pid, NULL, WUNTRACED);
    fg_pgid = -1;

    // TODO: fix this w/ shell struct
    Tcsetpgrp(STDIN_FILENO, shell_pgid);

    return EXEC_OK;
}

int run_builtin(Builtin b, Command *cmd) {
    if (b == NOT_A_BUILTIN)
        return -1;

    SavedFDs saved_fds;
    if (save_fds(&saved_fds) < 0) {
        return 1;
    }
    if (redirect_io(cmd) < 0) {
        restore_fds(&saved_fds);
        return 1;
    }

    int status;

    switch (b) {
    case BUILTIN_EXIT:
        free_cmd(cmd);
        exit(0);
        break;
    case BUILTIN_CD:
        status = builtin_cd(cmd);
        break;
    case BUILTIN_FG:
        status = builtin_fg(cmd);
        break;
    default:
        break;
    }

    if (status != EXEC_OK) {
        restore_fds(&saved_fds);
        return status;
    }

    if (restore_fds(&saved_fds) < 0) {
        return 1;
    }

    return EXEC_OK;
}