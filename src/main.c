#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "parse.h"
#include "sighandlers.h"
#include "syscall_wrappers.h"

#define MIN(x, y) (x) < (y) ? (x) : (y)

#define COL_GREEN "\033[32m"
#define COL_BLUE "\033[34m"
#define COL_CLR "\033[0m"

pid_t shell_pgid = -1;
pid_t fg_pgid = -1;
char cwd[100];

int redirect_io(Command *cmd) {
    for (size_t i = 0; i < cmd->n_redirects; i++) {
        Redirect redir = cmd->redirects[i];

        int fd;
        if ((fd = Open(redir.file, redir.o_flag, S_IRWXU)) < 0) {
            return -1;
        }

        if (Dup2(fd, redir.fd) < 0) {
            Close(fd);
            return -1;
        }

        if (Close(fd) < 0) {
            return -1;
        }
    }

    return 0;
}

int try_builtin(Command *cmd) {
    // TODO: redirect IO for builtins (and reset after)

    if (strcmp(cmd->argv[0], "exit") == 0) {
        free_cmd(cmd);
        exit(0);
    }

    if (strcmp(cmd->argv[0], "cd") == 0) {
        char *dst = cmd->argc > 1 ? cmd->argv[1] : getenv("HOME");
        if (!dst) return 0;

        if (Chdir(dst) < 0) {
            return 1;
        }

        if (!Getcwd(cwd, 100) && errno == ERANGE) {
            memcpy(cwd, "../", 4);
        }

        return 1;
    }

    if (strcmp(cmd->argv[0], "fg") == 0) {
        if (cmd->argc == 1) {
            printf("TODO: most recent job");
            return 1;
        }

        pid_t pid = strtol(cmd->argv[1], NULL, 10);

        kill(pid, SIGCONT);

        fg_pgid = pid;
        Waitpid(pid, NULL, WUNTRACED);
        fg_pgid = -1;

        Tcsetpgrp(STDIN_FILENO, shell_pgid);

        return 1;
    }

    return 0;
}

void exec_command(Command *cmd) {
    bool is_builtin = try_builtin(cmd);

    if (!is_builtin) {
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

            fg_pgid = pid;
            Waitpid(pid, NULL, WUNTRACED);
            fg_pgid = -1;

            Tcsetpgrp(STDIN_FILENO, shell_pgid);

        } else {
            printf("[%d]", pid);
            for (size_t i = 0; i < cmd->argc; i++) {
                printf(" %s", cmd->argv[i]);
            }
            printf("\n");
        }
    }
}

int main() {
    shell_pgid = getpgrp();

    if (!getcwd(cwd, 100)) {
        memcpy(cwd, "???", 4);
    }

    install_signal_handler(SIGCHLD, &reap_children);
    install_signal_handler(SIGINT, &keyboard_interrupt);
    install_signal_handler(SIGTSTP, &keyboard_interrupt);
    install_signal_handler(SIGTTOU, SIG_IGN);
    install_signal_handler(SIGTTIN, SIG_IGN);

    while (1) {
        printf(COL_GREEN "seashell" COL_CLR ":" COL_BLUE "%s" COL_CLR "$ ", cwd);

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

        exec_command(&cmd);

        free_cmd(&cmd);
        free(line);
    }

    return 0;
}