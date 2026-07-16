#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "syscall_wrappers.h"

void unix_error(char *msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

pid_t Fork() {
    pid_t pid;
    if ((pid = fork()) < 0) {
        unix_error("Fork error");
    }
    return pid;
}

int Execvp(const char *file, char *const *argv) {
    if (execvp(file, argv) < 0) {
        unix_error("Execve error");
        exit(-1);
    }
    return -1;
}

pid_t Waitpid(pid_t pid, int *stat_loc, int options) {
    if ((pid = waitpid(pid, stat_loc, options)) < 0) {
        unix_error("Waitpid error");
    }
    return pid;
}

int Tcsetpgrp(int fd, pid_t pgrp_id) {
    if (tcsetpgrp(fd, pgrp_id) < 0) {
        unix_error("Tcsetpgrp error");
        return -1;
    }
    return 0;
}

int Setpgid(pid_t pid, pid_t pgid) {
    if (setpgid(pid, pgid) < 0) {
        unix_error("Setpgid error");
        if (pid == 0) {
            exit(-1);
        }
        return -1;
    }
    return 0;
}

int Kill(pid_t pid, int sig) {
    if (kill(pid, sig) < 0) {
        unix_error("Kill error");
        return -1;
    }
    return 0;
}

int Chdir(const char *path) {
    if (chdir(path) < 0) {
        unix_error("cd");
        return -1;
    }
    return 0;
}

char *Getcwd(char *buf, size_t size) {
    char *cwd;
    if (!(cwd = getcwd(buf, size))) {
        if (errno != ERANGE) {
            unix_error("Getcwd error");
        }
    }
    return cwd;
}

int Open(char *file, int o_flag, int s_flag) {
    int fd;
    if ((fd = open(file, o_flag, s_flag)) < 0) {
        unix_error("File open error");
        return -1;
    }
    return fd;
}

int Close(int fd) {
    if (close(fd) < 0) {
        unix_error("File close error");
        return -1;
    }
    return 0;
}

int Dup(int fd) {
    int new_fd;
    if ((new_fd = dup(fd)) < 0) {
        unix_error("Redirection error");
        return -1;
    }
    return new_fd;
}

int Dup2(int fd, int fd2) {
    if (dup2(fd, fd2) < 0) {
        unix_error("Redirection error");
        return -1;
    }
    return 0;
}