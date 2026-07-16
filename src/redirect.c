#include "redirect.h"

#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "parse.h"
#include "syscall_wrappers.h"

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