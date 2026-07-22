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

void add_redirection(Command *cmd, char **next_token, int fd, int o_flag) {
    if (cmd->n_redirects >= MAX_REDIRECTS) {
        fprintf(stderr, "Warning: Max number of redirects exceeded; ignoring all after '%s'\n", cmd->redirects[cmd->n_redirects - 1].file);
    }

    Redirect redir = {.file = *next_token, .fd = fd, .o_flag = o_flag};
    cmd->redirects[cmd->n_redirects++] = redir;
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

int setup_pipe(int prev_pipe, int pipefd[2]) {
    if (prev_pipe > -1 && Dup2(prev_pipe, STDIN_FILENO) < 0) {
        Close(prev_pipe);
        return -1;
    }

    if (pipefd[1] > -1 && Dup2(pipefd[1], STDOUT_FILENO) < 0) {
        Close(pipefd[1]);
        return -1;
    }

    if (prev_pipe > -1 && Close(prev_pipe) < 0) {
        return -1;
    }
    if (pipefd[0] > -1) {
        int status = 0;
        status = Close(pipefd[0]);
        status = Close(pipefd[1]);
        if (status < 0) {
            return -1;
        }
    }

    return 0;
}

int close_pipe_read_end(int *prev_pipe, int pipefd[2]) {
    if (!prev_pipe) return -1;

    if (*prev_pipe > -1 && Close(*prev_pipe) < 0) {
        return -1;
    }

    if (pipefd[1] > -1) {
        if (Close(pipefd[1]) < 0) {
            return -1;
        }
        *prev_pipe = pipefd[0];
    } else {
        *prev_pipe = -1;
    }

    return 0;
}

int redirect_io(Command *cmd) {
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