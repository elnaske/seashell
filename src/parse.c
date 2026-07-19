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

void parse_error(int status) {
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

int parse_command(char ***p_next_token, char **argv_start, Command *cmd_out) {
    if (!cmd_out) return -1;

    Command cmd = {0};
    cmd.argv = argv_start;

    char **next_token = *p_next_token;

    while (*next_token) {
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

        if (r != REDIR_NONE) {
            next_token++;

            int status = parse_redirection(r, next_token, &cmd);
            if (status != PARSE_OK) {
                return status;
            }
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

int parse_line(char *line, size_t len, Job *job_out) {
    if (!job_out) return -1;
    
    size_t token_cnt;
    char **tokens = tokenize_line(line, len, &token_cnt);
    if (!tokens) {
        free(tokens);
        return PARSE_ERR_MALLOC;
    }
    if (!token_cnt) {
        free(tokens);
        return PARSE_OK;
    }

    char **argv = malloc(sizeof(tokens) * (token_cnt + 1));
    if (!argv) {
        free(tokens);
        return PARSE_ERR_MALLOC;
    }

    char **next_token = tokens;
    char **argv_start = argv;

    Job job = {0};
    job.run_in_bg = *(tokens[token_cnt - 1]) == '&';
    if (job.run_in_bg) {
        tokens[--token_cnt] = NULL;
    }

    while (*next_token) {
        Command cmd = {0};

        int status = parse_command(&next_token, argv_start, &cmd);
        if (status != PARSE_OK) {
            free(tokens);
            free(argv);
            return status;
        }

        argv_start += cmd.argc + 1;

        job.cmds[job.cmd_cnt++] = cmd;
    }

    *job_out = job;

    free(tokens);
    return PARSE_OK;
}
