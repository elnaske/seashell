#define _GNU_SOURCE

#include "sighandlers.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/shell.h"
#include "syscall_wrappers.h"

extern Shell shell;

volatile sig_atomic_t sigchld_received = 0;

void install_signal_handler(int signum, void (*handler)(int)) {
    struct sigaction act = {0};
    act.sa_handler = handler;

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    if (sigaction(signum, &act, NULL) < 0) {
        perror("Sigaction error");
        exit(1);
    }

    return;
}

void reap_children(Shell *s) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int job_id = jt_get_job_id(s, pid);
        if (job_id_is_valid(s, job_id)) {
            fprintf(stderr, "[%d] Done\n", job_id);
            jt_update_job_state(s, job_id, JOB_STATE_DONE);
        }
    }

    if (errno && errno != ECHILD) {
        perror("Waitpid error");
    }

    return;
}

void sigchld_handler(int sig) {
    (void)sig;

    sigchld_received = 1;
    return;
}

void keyboard_interrupt_handler(int sig) {
    if (shell.fg_pgid >= 0) {
        Kill(-shell.fg_pgid, sig);
        shell.fg_pgid = -1;
    }

    return;
}