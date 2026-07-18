#include "parse.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void free_cmd(Command *cmd) {
    if (!cmd) return;
    free(cmd->argv);
    cmd->argv = NULL;
    cmd->argc = 0;
}

typedef enum {
    REDIR_NONE,
    REDIR_STDIN,
    REDIR_STDOUT,
    REDIR_STDOUT_APPEND,
    REDIR_STDERR,
    REDIR_STDERR_APPEND,
    REDIR_BOTH,
    REDIR_BOTH_APPEND,
} RedirKind;

void parse_error(int status) {
    char *err;

    switch (status) {
    case PARSE_ERR_MALLOC:
        err = "memory allocation failure";
        break;
    case PARSE_ERR_FILENAME:
        err = "missing filename";
        break;
    default:
        return;
    }

    fprintf(stderr, "Parse error: %s\n", err);
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
            fprintf(stderr, "Warning: Max number of arguments exceeded; ignoring all after '%s'\n", tokens[token_cnt - 1]);
            break;
        }
    }

    if (cnt_out) {
        *cnt_out = token_cnt;
    }

    tokens[token_cnt] = NULL;

    return tokens;
}

static inline bool is_operator(char *s) {
    return strcmp(s, "<") == 0 || strcmp(s, ">") == 0 || strcmp(s, ">>") == 0 || strcmp(s, "2>") == 0 || strcmp(s, "2>>") == 0 || strcmp(s, "&>") == 0 || strcmp(s, "&>>") == 0;
}

int match_redirection(char *token) {
    if (strcmp(token, "<") == 0)
        return REDIR_STDIN;
    if (strcmp(token, ">") == 0)
        return REDIR_STDOUT;
    if (strcmp(token, ">>") == 0)
        return REDIR_STDOUT_APPEND;
    if (strcmp(token, "2>") == 0)
        return REDIR_STDERR;
    if (strcmp(token, "2>>") == 0)
        return REDIR_STDERR_APPEND;
    if (strcmp(token, "&>") == 0)
        return REDIR_BOTH;
    if (strcmp(token, "&>>") == 0)
        return REDIR_BOTH_APPEND;

    return REDIR_NONE;
}

void add_redirection(Command *cmd, char **next_token, int fd, int o_flag) {
    if (cmd->n_redirects >= MAX_REDIRECTS) {
        fprintf(stderr, "Warning: Max number of redirects exceeded; ignoring all after '%s'\n", cmd->redirects[cmd->n_redirects - 1].file);
    }

    Redirect redir = {.file = *next_token, .fd = fd, .o_flag = o_flag};
    cmd->redirects[cmd->n_redirects++] = redir;
}

int parse_redirection(RedirKind r, char **next_token, Command *cmd) {
    if (*next_token == NULL || is_operator(*next_token)) {
        return PARSE_ERR_FILENAME;
    }

    switch (r) {
    case REDIR_STDIN:
        add_redirection(cmd, next_token, STDIN_FILENO, O_RDONLY);
        break;
    case REDIR_STDOUT:
        add_redirection(cmd, next_token, STDOUT_FILENO, O_WRONLY | O_CREAT);
        break;
    case REDIR_STDOUT_APPEND:
        add_redirection(cmd, next_token, STDOUT_FILENO, O_WRONLY | O_CREAT | O_APPEND);
        break;
    case REDIR_STDERR:
        add_redirection(cmd, next_token, STDERR_FILENO, O_WRONLY | O_CREAT);
        break;
    case REDIR_STDERR_APPEND:
        add_redirection(cmd, next_token, STDERR_FILENO, O_WRONLY | O_CREAT | O_APPEND);
        break;
    case REDIR_BOTH:
        add_redirection(cmd, next_token, STDOUT_FILENO, O_WRONLY | O_CREAT);
        add_redirection(cmd, next_token, STDERR_FILENO, O_WRONLY | O_CREAT);
        break;
    case REDIR_BOTH_APPEND:
        add_redirection(cmd, next_token, STDOUT_FILENO, O_WRONLY | O_CREAT | O_APPEND);
        add_redirection(cmd, next_token, STDERR_FILENO, O_WRONLY | O_CREAT | O_APPEND);
        break;
    default:
        break;
    }
    return PARSE_OK;
}

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

    Command cmd = {0};
    cmd.argv = argv;
    cmd.run_in_bg = *(tokens[token_cnt - 1]) == '&';
    if (cmd.run_in_bg) {
        tokens[--token_cnt] = NULL;
    }

    char **next_token = tokens;
    while (*next_token) {
        RedirKind r = match_redirection(*next_token);

        if (r != REDIR_NONE) {
            next_token++;

            int status = parse_redirection(r, next_token, &cmd);
            if (status != PARSE_OK) {
                parse_error(status);

                free(tokens);
                free(argv);
                return status;
            }
        } else {
            cmd.argv[cmd.argc++] = *next_token;
        }

        next_token++;
    }

    cmd.argv[cmd.argc] = NULL;
    *cmd_out = cmd;

    free(tokens);
    return PARSE_OK;
}
