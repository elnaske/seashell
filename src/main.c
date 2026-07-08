#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#define MIN(x, y) (x) < (y) ? (x) : (y)
#define MAX_ARGS 8

typedef struct {
    char **argv;
    size_t argc;
} Command;

char **tokenize_line(char *line, size_t len, size_t *cnt_out) {
    if (!line) return NULL;

    char **tokens = malloc(sizeof(char *) * (MAX_ARGS + 1)); // terminated by NULL ptr
    if (!tokens) return NULL;

    size_t token_cnt = 0;

    char *curr_token = NULL;

    for (size_t i = 0; i < len; i++) {
        char c = line[i];
 
        if (isspace(c) && curr_token) {
            line[i] = '\0';
            tokens[token_cnt++] = curr_token;
            curr_token = NULL;

            if (token_cnt >= MAX_ARGS) {
                printf("Warning: Max number of arguments exceeded; ignoring all after '%s'\n", tokens[token_cnt - 1]);
                break;
            }
        }
        else {
            if (!curr_token) {
                curr_token = line + i;
            }
        }
    }

    if (cnt_out) {
        *cnt_out = token_cnt;
    }

    return tokens;
}

Command parse_line(char **tokens, size_t token_cnt) {
    // TODO: pipes, redirection, etc.

    tokens[token_cnt] = NULL;
    
    Command cmd = {.argv = tokens, .argc = token_cnt};
    return cmd;
}

int try_builtin(Command cmd) {
    if (strcmp(cmd.argv[0], "exit") == 0) {
        free(cmd.argv);
        exit(0);
    }

    return 0;
}

int main() {
    char *line = NULL;
    char **args = NULL;

    while (1) {
        printf("seashell> ");

        line = NULL;
        size_t len = 0;
        ssize_t n_read = getline(&line, &len, stdin);

        if (n_read == -1) {
            return -1;
        }

        size_t arg_cnt;
        args = tokenize_line(line, len, &arg_cnt);
        if (!args) {
            return -1;
        }

        if (arg_cnt) {
            Command cmd = parse_line(args, arg_cnt);

            if (!try_builtin(cmd)) {
                pid_t pid = fork();
                if (pid < 0) {
                    printf("Fork error: %s\n", strerror(errno));
                    continue;
                }

                if (pid == 0) {
                    if (execve(cmd.argv[0], cmd.argv, NULL) < 0) {
                        printf("Execve error: %s\n", strerror(errno));
                        exit(errno);
                    }

                    exit(0);
                }

                if (waitpid(pid, NULL, 0) < 0) {
                    printf("Waitpid error: %s\n", strerror(errno));
                }
            }

        }

        free(args);
        free(line);
    }
    free(args);
    free(line);

    return 0;
}