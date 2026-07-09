#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sighandlers.h"

#define MIN(x, y) (x) < (y) ? (x) : (y)
#define MAX_ARGS 8

typedef struct {
    char **argv;
    size_t argc;
    bool run_in_bg;
} Command;

static inline void free_cmd(Command *cmd) {
    if (!cmd) return;
    free(cmd->argv);
    cmd->argv = NULL;
    cmd->argc = 0;
}

char **tokenize_line(char *line, size_t len, size_t *cnt_out) {
    if (!line) return NULL;

    char **tokens = malloc(sizeof(char *) * (MAX_ARGS + 1)); // terminated by NULL ptr
    if (!tokens) return NULL;

    size_t token_cnt = 0;

    size_t idx = 0;

    while (idx < len && line[idx] != '\0') {
        while (idx < len && isspace(line[idx])) {
            idx++;
        }

        if (line[idx] == '\0') {
            break;
        }

        size_t start = idx;

        while (line[idx] != '\0' && !isspace(line[idx])) {
            idx++;
        }

        line[idx++] = '\0';
        tokens[token_cnt++] = line + start;

        if (token_cnt >= MAX_ARGS) {
            printf("Warning: Max number of arguments exceeded; ignoring all after '%s'\n", tokens[token_cnt - 1]);
            break;
        }
    }

    if (cnt_out) {
        *cnt_out = token_cnt;
    }

    tokens[token_cnt] = NULL;

    return tokens;
}

int parse_line(char *line, size_t len, Command *cmd_out) {
    if (!cmd_out) return -1;

    size_t token_cnt;
    char **tokens = tokenize_line(line, len, &token_cnt);

    if (!tokens) {
        return -1;
    }

    // TODO: pipes, redirection, etc.

    bool run_in_bg = *(tokens[token_cnt - 1]) == '&';
    if (run_in_bg) {
        tokens[--token_cnt] = NULL;
    }

    Command cmd = {
        .argv = tokens,
        .argc = token_cnt,
        .run_in_bg = run_in_bg,
    };

    *cmd_out = cmd;

    return 0;
}

int try_builtin(Command cmd) {
    if (strcmp(cmd.argv[0], "exit") == 0) {
        free(cmd.argv);
        exit(0);
    }
    // TODO: other builtins

    return 0;
}

void exec_command(Command cmd) {
    bool is_builtin = try_builtin(cmd);

    if (!is_builtin) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("Fork error: %s\n", strerror(errno));
            return;
        }

        if (pid == 0) {
            if (execvp(cmd.argv[0], cmd.argv) < 0) {
                printf("Execve error: %s\n", strerror(errno));
                exit(errno);
            }

            exit(0);
        }

        if (!cmd.run_in_bg) {
            if (waitpid(pid, NULL, 0) < 0) {
                printf("Waitpid error: %s\n", strerror(errno));
            }
        } else {
            printf("[%d]", pid);
            for (size_t i = 0; i < cmd.argc; i++) {
                printf(" %s", cmd.argv[i]);
            }
            printf("\n");
        }
    }
}

int main() {
    install_signal_handler(SIGCHLD, &reap_children);

    while (1) {
        printf("seashell> ");

        char *line = NULL;
        size_t len = 0;
        ssize_t n_read = getline(&line, &len, stdin);

        if (n_read == -1) {
            return -1;
        }

        Command cmd;
        if (parse_line(line, len, &cmd) < 0) {
            free(line);
            return -1;
        }

        if (cmd.argc) {
            exec_command(cmd);
        }

        free_cmd(&cmd);
        free(line);
    }

    return 0;
}