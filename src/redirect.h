#pragma once

typedef struct Command Command;

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

typedef struct Redirect {
    char *file;
    int fd;
    int o_flag;
} Redirect;

typedef struct {
    int stdin;
    int stdout;
    int stderr;
} SavedFDs;

int match_redirection(char *token);

void add_redirection(Command *cmd, char **next_token, int fd, int o_flag);

int save_fds(SavedFDs *fd_out);

int restore_fds(SavedFDs *saved);

int redirect_io(Command *cmd);