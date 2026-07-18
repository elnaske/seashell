#include "shell.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "parse.h"
#include "redirect.h"
#include "sighandlers.h"
#include "syscall_wrappers.h"

#define COL_GREEN "\033[32m"
#define COL_BLUE "\033[34m"
#define COL_CLR "\033[0m"

int shell_init(Shell *s) {
    s->pgid = getpgrp();
    s->fg_pgid = -1;

    if (!getcwd(s->cwd, MAX_PATHNAME_LENGTH)) {
        memcpy(s->cwd, "../", 4);
    }

    install_signal_handler(SIGCHLD, &reap_children);
    install_signal_handler(SIGINT, &keyboard_interrupt);
    install_signal_handler(SIGTSTP, &keyboard_interrupt);
    install_signal_handler(SIGTTOU, SIG_IGN);
    install_signal_handler(SIGTTIN, SIG_IGN);

    return 0;
}

void exec_command(Shell *s, Command *cmd) {
    // bool is_builtin = try_builtin(cmd);
    Builtin b = match_builtin(cmd);

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

int shell_run(Shell *s) {
    while (1) {
        printf(COL_GREEN "seashell" COL_CLR ":" COL_BLUE "%s" COL_CLR "$ ", s->cwd);

        char *line = NULL;
        size_t len = 0;
        ssize_t n_read = getline(&line, &len, stdin);

        if (n_read == -1) {
            free(line);
            return -1;
        }

        Command cmd;
        if (parse_line(line, len, &cmd) != PARSE_OK) {
            free(line);
            continue;
        }

        exec_command(s, &cmd);

        free_cmd(&cmd);
        free(line);
    }
    return 0;
}