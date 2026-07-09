#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include "sighandlers.h"

void install_signal_handler(int signum, void (*handler)(int)) {
    struct sigaction act = {0};
    act.sa_handler = handler;

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    if (sigaction(signum, &act, NULL) < 0) {
        printf("Sigaction error: %s", strerror(errno));
    }

    return;
}

void reap_children(int sig) {
    (void)sig;

    int saved_errno = errno;
    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Reaped process %d\n", pid);
    }

    if (errno && errno != ECHILD) {
        printf("Waitpid error: %s\n", strerror(errno));
    }

    errno = saved_errno;

    return;
}