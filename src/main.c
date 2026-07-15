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

#include "sighandlers.h"
#include "syscall_wrappers.h"

#define MIN(x, y) (x) < (y) ? (x) : (y)
#define MAX_ARGS 8

#define COL_GREEN "\033[32m"
#define COL_BLUE "\033[34m"
#define COL_CLR "\033[0m"

pid_t shell_pgid = -1;
pid_t fg_pgid = -1;
char cwd[100];

typedef struct {
    char **argv;
    size_t argc;
    char *stdin_redirect;
    char *stdout_redirect;
    char *stderr_redirect;
    bool stdout_append;
    bool stderr_append;
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

static inline bool is_operator(char c) {
    return c == '<' || c == '>';
}

typedef enum {
    PARSE_OK,
    PARSE_ERR_MALLOC,
    PARSE_ERR_FILENAME,
} PARSE_STATUS;

int parse_line(char *line, size_t len, Command *cmd_out) {
    if (!cmd_out) return -1;

    size_t token_cnt;
    char **tokens = tokenize_line(line, len, &token_cnt);

    if (!tokens || !token_cnt) {
        free(tokens);
        return PARSE_ERR_MALLOC;
    }

    char **argv = malloc(sizeof(tokens) * (token_cnt + 1));
    if (!argv) {
        free(tokens);
        return PARSE_ERR_MALLOC;
    }

    Command cmd = {
        .argv = argv,
        .argc = 0,
        .stdin_redirect = NULL,
        .stdout_redirect = NULL,
        .stderr_redirect = NULL,
        .stdout_append = false,
        .stderr_append = false,
        .run_in_bg = *(tokens[token_cnt - 1]) == '&',
    };

    if (cmd.run_in_bg) {
        tokens[--token_cnt] = NULL;
    }

    char **next_token = tokens;
    while (*next_token) {
        switch (**next_token) {
        case '<':
            if (strncmp(*next_token, "<", 2) == 0) {
                next_token++;
                if (*next_token == NULL || is_operator(**next_token)) {
                    free(tokens);
                    return PARSE_ERR_FILENAME;
                }
                cmd.stdin_redirect = *next_token;
                break;
            }
            continue;
        case '>':
            if (strncmp(*next_token, ">", 2) == 0 || strncmp(*next_token, ">>", 3) == 0) {
                cmd.stdout_append = strncmp(*next_token, ">>", 3) == 0;

                next_token++;
                if (*next_token == NULL || is_operator(**next_token)) {
                    free(tokens);
                    return PARSE_ERR_FILENAME;
                }
                cmd.stdout_redirect = *next_token;
                break;
            }
            continue;
        case '2':
            if (strncmp(*next_token, "2>", 3) == 0 || strncmp(*next_token, "2>>", 4) == 0) {
                cmd.stderr_append = strncmp(*next_token, "2>>", 4) == 0;
                
                next_token++;
                if (*next_token == NULL || is_operator(**next_token)) {
                    free(tokens);
                    return PARSE_ERR_FILENAME;
                }
                cmd.stderr_redirect = *next_token;
                break;
            }
            continue;
        case '&':
            if (strncmp(*next_token, "&>", 3) == 0 || strncmp(*next_token, "&>>", 4) == 0) {
                cmd.stdout_append = strncmp(*next_token, "&>>", 4) == 0;
                cmd.stderr_append = cmd.stdout_append;
                
                next_token++;
                if (*next_token == NULL || is_operator(**next_token)) {
                    free(tokens);
                    return PARSE_ERR_FILENAME;
                }
                cmd.stdout_redirect = *next_token;
                cmd.stderr_redirect = *next_token;
                break;
            }
            continue;
        default:
            cmd.argv[cmd.argc++] = *next_token;
            break;
        }
        next_token++;
    }

    cmd.argv[cmd.argc] = NULL;

    free(tokens);

    *cmd_out = cmd;

    return 0;
}

int try_builtin(Command *cmd) {
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

int redirect_io(char *file, int io_fd, int o_flag, int s_flag) {
    int fd;
    if ((fd = Open(file, o_flag, s_flag)) < 0) {
        return -1;
    }

    if (Dup2(fd, io_fd) < 0) {
        Close(fd);
        return -1;
    }

    if (Close(fd) < 0) {
        return -1;
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

            if (cmd->stdout_redirect) {
                int o_append = cmd->stdout_append ? O_APPEND : 0;
                if (redirect_io(cmd->stdout_redirect, STDOUT_FILENO, O_WRONLY | O_CREAT | o_append, S_IRWXU) < 0) {
                    exit(-1);
                }
            }
            if (cmd->stderr_redirect) {
                int o_append = cmd->stdout_append ? O_APPEND : 0;
                if (redirect_io(cmd->stderr_redirect, STDERR_FILENO, O_WRONLY | O_CREAT | o_append, S_IRWXU) < 0) {
                    exit(-1);
                }
            }
            if (cmd->stdin_redirect) {
                if (redirect_io(cmd->stdin_redirect, STDIN_FILENO, O_RDONLY, 0) < 0) {
                    exit(-1);
                }
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
        if (parse_line(line, len, &cmd) < 0) {
            free(line);
            continue;
        }

        exec_command(&cmd);

        free_cmd(&cmd);
        free(line);
    }

    return 0;
}