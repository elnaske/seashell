#include "parse.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "options.h"
#include "redirect.h"
#include "shell.h"

void print_syntax_error(int status) {
    char *err;

    switch (status) {
    case PARSE_ERR_MALLOC:
        err = "memory allocation failure";
        break;
    case PARSE_ERR_FILENAME:
        err = "missing filename";
        break;
    case PARSE_ERR_LEADING_PIPE:
        err = "leading pipe";
        break;
    case PARSE_ERR_DANGLING_PIPE:
        err = "dangling pipe";
        break;
    case PARSE_ERR_AMPERSAND:
        err = "non-final '&'";
        break;
    default:
        return;
    }

    fprintf(stderr, "Syntax error: %s\n", err);
}

static inline bool is_operator(char *s) {
    return strcmp(s, "<") == 0 || strcmp(s, ">") == 0 || strcmp(s, ">>") == 0 || strcmp(s, "2>") == 0 || strcmp(s, "2>>") == 0 || strcmp(s, "&>") == 0 || strcmp(s, "&>>") == 0;
}

static inline bool is_redirection(RedirKind r) {
    return r != REDIR_NONE;
}

static inline bool is_var(char **next_token) {
    return (*next_token)[0] == '$' && (*next_token)[1] != '\0';
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

int parse_redirection(RedirKind r, char **next_token, Command *cmd) {
    if (*next_token == NULL || is_operator(*next_token) || strcmp(*next_token, "|") == 0) {
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

char *expand_var(char **next_token, char *exit_code_start) {
    char *expanded;

    if (strcmp(*next_token, "$?") == 0) {
        expanded = exit_code_start;
    } else {
        char *env_var = getenv(*next_token + 1); // skip '$'
        if (env_var) {
            expanded = env_var;
        } else {
            expanded = "";
        }
    }
    return expanded;
}

int parse_command(char ***p_next_token, char **argv_start, char *exit_code_start, Command *cmd_out) {
    if (!cmd_out) return -1;

    Command cmd = {0};
    cmd.argv = argv_start;

    char **next_token = *p_next_token;

    while (*next_token) {
        if (strcmp(*next_token, "&") == 0) {
            next_token++;
            // final ampersand is removed before parsing, so any ampersand is out of place
            // once multiple jobs per line are supported, this will no return an error and instead begin a new job
            return PARSE_ERR_AMPERSAND;
        }

        if (strcmp(*next_token, "|") == 0) {
            next_token++;

            if (cmd.argc == 0) {
                return PARSE_ERR_LEADING_PIPE;
            }

            if (!*next_token) {
                return PARSE_ERR_DANGLING_PIPE;
            }
            break;
        }

        RedirKind r = match_redirection(*next_token);
        if (is_redirection(r)) {
            next_token++;

            int status = parse_redirection(r, next_token, &cmd);
            if (status != PARSE_OK) {
                return status;
            }
        } else if (is_var(next_token)) {
            cmd.argv[cmd.argc++] = expand_var(next_token, exit_code_start);
        } else {
            cmd.argv[cmd.argc++] = *next_token;
        }

        next_token++;
    }

    cmd.argv[cmd.argc] = NULL;

    if (cmd_out) {
        *cmd_out = cmd;
    }

    *p_next_token = next_token;

    return PARSE_OK;
}

int parse_line(Shell *s, char *line, size_t len, Job *job_out) {
    if (!job_out) return -1;

    size_t token_cnt;
    char **tokens = tokenize_line(line, len, &token_cnt);
    if (!tokens) {
        return PARSE_ERR_MALLOC;
    }

    if (token_cnt) {
        size_t max_argv_len = sizeof(tokens) * (token_cnt + 1);
        size_t exit_code_str_len = 4; // three digits (8-bits) + null terminator

        /*
         * Arena allocation that holds args (pointers into line), a NULL separator, and the previous exit code (last 4 bytes; for expanding $?)
         * i.e.
         * arg_arena: [[argv pointers], NULL, [padding], "130"]]
         *             |  |   |
         * line:      [..0...0.....0]
         */
        void *arg_arena = malloc(max_argv_len + exit_code_str_len);
        if (!arg_arena) {
            free(tokens);
            return PARSE_ERR_MALLOC;
        }

        char **argv_start = arg_arena;
        char *exit_code_start = arg_arena + max_argv_len;

        snprintf(exit_code_start, exit_code_str_len, "%d", s->last_status);

        Job job = {0};
        job.prev_pipe = -1;
        job.run_in_bg = *(tokens[token_cnt - 1]) == '&';
        if (job.run_in_bg) {
            tokens[--token_cnt] = NULL;
        }

        char **next_token = tokens;
        while (*next_token) {
            Command cmd = {0};

            int status = parse_command(&next_token, argv_start, exit_code_start, &cmd);
            if (status != PARSE_OK) {
                free(tokens);
                free(arg_arena);
                return status;
            }

            argv_start += cmd.argc + 1;

            job.cmds[job.cmd_cnt++] = cmd;
        }

        *job_out = job;
    }

    free(tokens);
    return PARSE_OK;
}
