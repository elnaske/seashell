#include "parse.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../cmd/commands.h"
#include "../cmd/pipeline.h"
#include "../core/shell.h"
#include "../options.h"
#include "../sys/redirect.h"
#include "tokenize.h"

static inline bool is_operator(char *s) {
    return strcmp(s, "<") == 0 || strcmp(s, ">") == 0 || strcmp(s, ">>") == 0 || strcmp(s, "2>") == 0 || strcmp(s, "2>>") == 0 || strcmp(s, "&>") == 0 || strcmp(s, "&>>") == 0;
}

static inline bool is_redirection(RedirKind r) {
    return r != REDIR_NONE;
}

static inline bool is_var(char *s) {
    return s[0] == '$' && s[1] != '\0';
}

int parse_redirection(RedirKind r, char **next_token, Command *cmd) {
    if (*next_token == NULL || is_operator(*next_token) || strcmp(*next_token, "|") == 0) {
        return PARSE_ERR_FILENAME;
    }

    int fd[2] = {-1, -1};
    int o_flag;

    switch (r) {
    case REDIR_STDIN:
        fd[0] = STDIN_FILENO;
        o_flag = O_RDONLY;
        break;
    case REDIR_STDOUT:
        fd[0] = STDOUT_FILENO;
        o_flag = O_WRONLY | O_CREAT;
        break;
    case REDIR_STDOUT_APPEND:
        fd[0] = STDOUT_FILENO;
        o_flag = O_WRONLY | O_CREAT | O_APPEND;
        break;
    case REDIR_STDERR:
        fd[0] = STDERR_FILENO;
        o_flag = O_WRONLY | O_CREAT;
        break;
    case REDIR_STDERR_APPEND:
        fd[0] = STDERR_FILENO;
        o_flag = O_WRONLY | O_CREAT | O_APPEND;
        break;
    case REDIR_BOTH:
        fd[0] = STDOUT_FILENO;
        fd[1] = STDERR_FILENO;
        o_flag = O_WRONLY | O_CREAT;
        break;
    case REDIR_BOTH_APPEND:
        fd[0] = STDOUT_FILENO;
        fd[1] = STDERR_FILENO;
        o_flag = O_WRONLY | O_CREAT | O_APPEND;
        break;
    default:
        break;
    }

    cmd_add_redirection(cmd, *next_token, fd[0], o_flag);

    if (fd[1] >= 0) {
        cmd_add_redirection(cmd, *next_token, fd[1], o_flag);
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
    if (!p_next_token || !argv_start || !exit_code_start || !cmd_out) return -1;

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
        } else if (is_var(*next_token)) {
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


int parse_line(Shell *s, char *line, size_t len, Pipeline *pl_out) {
    if (!s || !line || !pl_out) return -1;

    size_t token_cnt;
    char **tokens = tokenize_line(line, len, &token_cnt);
    if (!tokens) {
        return PARSE_ERR_MALLOC;
    }

    if (token_cnt) {
        ArgArena mem_arena;
        if (init_arg_arena(&mem_arena, tokens, token_cnt, len, s->last_status) < 0) {
            free(tokens);
            return PARSE_ERR_MALLOC;
        }

        Pipeline pl = {0};
        pl.prev_pipe = -1;
        pl.cmd_line = mem_arena.cmd_line;

        pl.run_in_bg = *(tokens[token_cnt - 1]) == '&';
        if (pl.run_in_bg) {
            tokens[--token_cnt] = NULL;
        }

        char **next_token = tokens;
        char **argv_start = mem_arena.args;
        while (*next_token) {
            Command cmd = {0};

            int status = parse_command(&next_token, argv_start, mem_arena.last_status, &cmd);
            if (status != PARSE_OK) {
                free(tokens);
                free_pipeline(&pl);
                return status;
            }

            argv_start += cmd.argc + 1;

            pl.cmds[pl.cmd_cnt++] = cmd;
        }

        *pl_out = pl;
    }

    free(tokens);
    return PARSE_OK;
}

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