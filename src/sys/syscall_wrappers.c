#include "syscall_wrappers.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t Fork() {
    pid_t pid;
    if ((pid = fork()) < 0) {
        perror("Fork error");
    }
    return pid;
}

int Execvp(const char *file, char *const *argv) {
    if (execvp(file, argv) < 0) {
        perror("Execve error");
        exit(-1);
    }
    return -1;
}

pid_t Waitpid(pid_t pid, int *stat_loc, int options) {
    if ((pid = waitpid(pid, stat_loc, options)) < 0) {
        perror("Waitpid error");
    }
    return pid;
}

int Tcsetpgrp(int fd, pid_t pgrp_id) {
    if (isatty(fd) && tcsetpgrp(fd, pgrp_id) < 0) {
        perror("Tcsetpgrp error");
        return -1;
    }
    return 0;
}

int Setpgid(pid_t pid, pid_t pgid) {
    if (setpgid(pid, pgid) < 0) {
        perror("Setpgid error");
        if (pid == 0) {
            exit(-1);
        }
        return -1;
    }
    return 0;
}

int Kill(pid_t pid, int sig) {
    if (kill(pid, sig) < 0) {
        perror("Kill error");
        return -1;
    }
    return 0;
}

int Chdir(const char *path) {
    if (chdir(path) < 0) {
        perror("cd");
        return -1;
    }
    return 0;
}

char *Getcwd(char *buf, size_t size) {
    char *cwd;
    if (!(cwd = getcwd(buf, size))) {
        if (errno != ERANGE) {
            perror("Getcwd error");
        }
    }
    return cwd;
}

int Open(char *file, int o_flag, int s_flag) {
    int fd;
    if ((fd = open(file, o_flag, s_flag)) < 0) {
        perror("File open error");
        return -1;
    }
    return fd;
}

int Close(int fd) {
    if (close(fd) < 0) {
        perror("File close error");
        return -1;
    }
    return 0;
}

int Dup(int fd) {
    int new_fd;
    if ((new_fd = dup(fd)) < 0) {
        perror("Redirection error");
        return -1;
    }
    return new_fd;
}

int Dup2(int fd, int fd2) {
    if (dup2(fd, fd2) < 0) {
        perror("Redirection error");
        return -1;
    }
    return 0;
}

int Pipe(int pipefd[2]) {
    if (pipe(pipefd) < 0) {
        perror("Pipe error");
        return -1;
    }
    return 0;
}