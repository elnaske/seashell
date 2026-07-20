#include "shell.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "parse.h"
#include "redirect.h"
#include "sighandlers.h"
#include "syscall_wrappers.h"

#define COL_GREEN "\033[32m"
#define COL_BLUE "\033[34m"
#define COL_CLR "\033[0m"

extern volatile sig_atomic_t sigchld_received;

int shell_init(Shell *s) {
    s->pgid = getpgrp();
    s->fg_pgid = -1;
    s->last_status = 0;

    if (!getcwd(s->cwd, MAX_PATHNAME_LENGTH)) {
        memcpy(s->cwd, "../", 4);
    }

    install_signal_handler(SIGCHLD, &sigchld_handler);
    install_signal_handler(SIGINT, &keyboard_interrupt_handler);
    install_signal_handler(SIGTSTP, &keyboard_interrupt_handler);
    install_signal_handler(SIGTTOU, SIG_IGN);
    install_signal_handler(SIGTTIN, SIG_IGN);

    s->running = true;

    return 0;
}

int shell_run(Shell *s) {
    while (s->running) {
        printf(COL_GREEN "seashell" COL_CLR ":" COL_BLUE "%s" COL_CLR "$ ", s->cwd);

        char *line = NULL;
        size_t len = 0;
        ssize_t n_read = getline(&line, &len, stdin);

        if (n_read == -1) {
            free(line);
            return -1;
        }

        Job job = {0};
        int status = parse_line(line, len, &job);
        if (status != PARSE_OK) {
            print_syntax_error(status);
            s->last_status = status;
            free(line);
            continue;
        }

        status = run_job(s, &job);
        // if (s->running)
            s->last_status = status;

        if (sigchld_received) {
            reap_children();
            sigchld_received = 0;
        }

        free_job(&job);
        free(line);
    }
    return s->last_status;
}