#pragma once

typedef struct Command Command;
typedef struct Pipeline Pipeline;

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

int save_fds(SavedFDs *fd_out);

int restore_fds(SavedFDs *saved);

int apply_redirections(Command *cmd);

int apply_pipe(int prev_pipe, int read_fd, int write_fd);

int update_pipe_read_end(Pipeline *pl, int read_fd, int write_fd);
