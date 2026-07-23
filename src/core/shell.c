#include "shell.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../cmd/commands.h"
#include "../parser/parse.h"
#include "../sys/redirect.h"
#include "../sys/sighandlers.h"
#include "../sys/syscall_wrappers.h"
#include "jobs.h"

#define COL_GREEN "\033[32m"
#define COL_BLUE "\033[34m"
#define COL_CLR "\033[0m"

extern volatile sig_atomic_t sigchld_received;

int shell_init(Shell *s) {
    void *jt = calloc(MAX_JOBS, sizeof(JobTableEntry));
    void *cmd_lines = malloc(MAX_JOBS * (MAX_CMD_LINE_LEN + 1) * sizeof(char));
    if (!jt || !cmd_lines) {
        return -1;
    }
    s->jt.jobs = jt;
    s->jt.cmd_lines = cmd_lines;
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

static inline void set_last_status(Shell *s, int status) {
    s->last_status = (uint8_t)abs(status);
}

int shell_run(Shell *s) {
    while (s->running) {
        printf(COL_GREEN "seashell" COL_CLR ":" COL_BLUE "%s" COL_CLR "$ ", s->cwd);

        char *line = NULL;
        size_t len = 0;
        int n_read = getline(&line, &len, stdin);

        if (n_read == -1) {
            free(line);
            return -1;
        }

        Pipeline pl = {0};
        int status = parse_line(s, line, len, &pl);

        if (status == PARSE_OK) {
            status = run_pipeline(s, &pl);
        } else {
            print_syntax_error(status);
        }

        set_last_status(s, status);

        if (sigchld_received) {
            reap_children();
            sigchld_received = 0;
        }

        jt_clear_finished_jobs(s);

        free_pipeline(&pl);
        free(line);
    }
    free_jt(s);
    return s->last_status;
}