#define _GNU_SOURCE

#include "sighandlers.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "syscall_wrappers.h"

extern Shell shell;

volatile sig_atomic_t sigchld_received = 0;

void install_signal_handler(int signum, void (*handler)(int)) {
    struct sigaction act = {0};
    act.sa_handler = handler;

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    if (sigaction(signum, &act, NULL) < 0) {
        fprintf(stderr, "Sigaction error: %s", strerror(errno));
        exit(1);
    }

    return;
}

void reap_children() {
    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        fprintf(stderr, "Reaped process %d\n", pid);
    }

    if (errno && errno != ECHILD) {
        fprintf(stderr, "Waitpid error: %s\n", strerror(errno));
    }

    return;
}

void sigchld_handler(int sig) {
    (void)sig;

    sigchld_received = 1;
}

void keyboard_interrupt_handler(int sig) {
    if (shell.fg_pgid >= 0) {
        Kill(-shell.fg_pgid, sig);
        shell.fg_pgid = -1;
    }

    return;
}