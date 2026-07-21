#include "shell.h"

#include <signal.h>
#include <stdint.h>
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
    JobTableEntry *jt = calloc(MAX_JOBS, sizeof(JobTableEntry));
    if (!jt) {
        return -1;
    }
    s->job_table = jt;
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

char *get_job_state_str(JobState js) {
    switch (js) {
    case JOB_STATE_FREE:
        return "Free";
    case JOB_STATE_FG:
        return "Running (fg)";
    case JOB_STATE_BG:
        return "Running (bg)";
    case JOB_STATE_STOPPED:
        return "Stopped";
    case JOB_STATE_DONE:
        return "Done";
    }
    return "n/a";
}

int add_job_table_entry(Shell *s, Job *job) {
    JobTableEntry jte = {
        .cmd_cnt = job->cmd_cnt,
        .pgid = job->pgid,
        .last_pid = job->last_pid,
        .state = job->run_in_bg ? JOB_STATE_BG : JOB_STATE_FG,
    };

    for (size_t i = 0; i < MAX_JOBS; i++) {
        if (s->job_table[i].state == JOB_STATE_FREE) {
            s->job_table[i] = jte;
            return i;
        }
    }
    return -1;
}

inline bool is_job_id_valid(Shell *s, int job_id) {
    return job_id >= 0 && job_id < MAX_JOBS && s->job_table[job_id].state != JOB_STATE_FREE;
}

bool job_table_is_full(Shell *s) {
    for (size_t i = 0; i < MAX_JOBS; i++) {
        if (s->job_table[i].state == JOB_STATE_FREE) {
            return false;
        }
    }
    return true;
}

// int job_table_update_state(Shell *s, int job_id, int status) {
//     if (!is_job_id_valid(s, job_id)) return -1;

//     if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGTSTP || WTERMSIG(status) == SIGSTOP)) {
//         // TODO: fix this
//         s->job_table[job_id].state = JOB_STATE_STOPPED;
//     } else {
//         s->job_table[job_id].state = JOB_STATE_DONE;
//     }

//     return 0;
// }
int job_table_update_state(Shell *s, int job_id, int state) {
    if (!is_job_id_valid(s, job_id)) return -1;
    s->job_table[job_id].state = state;

    return 0;
}

int job_table_find_job_id(Shell *s, pid_t pgid) {
    for (size_t i = 0; i < MAX_JOBS; i++) {
        if (s->job_table[i].pgid == pgid) {
            return i;
        }
    }
    return -1;
}

int job_table_mark_finished(Shell *s, pid_t pid) {
    for (size_t i = 0; i < MAX_JOBS; i++) {
        if (s->job_table[i].last_pid == pid) {
            s->job_table[i].state = JOB_STATE_DONE;
            return i;
        }
    }
    return -1;
}

void job_table_mark_free(Shell *s) {
    for (size_t i = 0; i < MAX_JOBS; i++) {
        if (s->job_table[i].state == JOB_STATE_DONE) {
            s->job_table[i].state = JOB_STATE_FREE;
        }
    }
    return;
}

void job_table_print(Shell *s) {
    for (size_t i = 0; i < MAX_JOBS; i++) {
        JobTableEntry jte = s->job_table[i];
        if (jte.state != JOB_STATE_FREE) {
            printf("[%ld] %s\t\t%d\n", i, get_job_state_str(jte.state), jte.pgid);
        }
    }
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

        Job job = {0};
        int status = parse_line(s, line, len, &job);
        if (status != PARSE_OK) {
            print_syntax_error(status);
            set_last_status(s, status);
            free(line);
            continue;
        }

        status = run_job(s, &job);
        set_last_status(s, status);

        if (sigchld_received) {
            reap_children();
            sigchld_received = 0;
        }

        job_table_mark_free(s);

        free_job(&job);
        free(line);
    }
    free(s->job_table);
    return s->last_status;
}