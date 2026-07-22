#include "redirect.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../cmd/commands.h"
#include "../parser/parse.h"
#include "../sys/syscall_wrappers.h"

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

int save_fds(SavedFDs *fd_out) {
    SavedFDs saved_fds;

    if ((saved_fds.stdin = Dup(STDIN_FILENO)) < 0) {
        return -1;
    }
    if ((saved_fds.stdout = Dup(STDOUT_FILENO)) < 0) {
        return -1;
    }
    if ((saved_fds.stderr = Dup(STDERR_FILENO)) < 0) {
        return -1;
    }

    *fd_out = saved_fds;
    return 0;
}

int restore_fds(SavedFDs *saved) {
    int return_val = 0;

    if (Dup2(saved->stdin, STDIN_FILENO) < 0) {
        return_val = -1;
    }
    if (Dup2(saved->stdout, STDOUT_FILENO) < 0) {
        return_val = -1;
    }
    if (Dup2(saved->stderr, STDERR_FILENO) < 0) {
        return_val = -1;
    }

    if (Close(saved->stdin) < 0) {
        return_val = -1;
    }
    if (Close(saved->stdout) < 0) {
        return_val = -1;
    }
    if (Close(saved->stderr) < 0) {
        return_val = -1;
    }

    return return_val;
}

int apply_redirections(Command *cmd) {
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

int apply_pipe(int prev_pipe, int read_fd, int write_fd) {
    if (prev_pipe > -1 && Dup2(prev_pipe, STDIN_FILENO) < 0) {
        Close(prev_pipe);
        return -1;
    }

    if (read_fd > -1 && Dup2(read_fd, STDOUT_FILENO) < 0) {
        Close(read_fd);
        return -1;
    }

    if (prev_pipe > -1 && Close(prev_pipe) < 0) {
        return -1;
    }
    if (write_fd > -1) {
        int status = 0;
        status = Close(write_fd);
        status = Close(read_fd);
        if (status < 0) {
            return -1;
        }
    }

    return 0;
}

int update_pipe_read_end(int *prev_pipe, int read_fd, int write_fd) {
    if (!prev_pipe) return -1;

    if (*prev_pipe > -1 && Close(*prev_pipe) < 0) {
        return -1;
    }

    if (read_fd > -1) {
        if (Close(read_fd) < 0) {
            return -1;
        }
        *prev_pipe = write_fd;
    } else {
        *prev_pipe = -1;
    }

    return 0;
}
