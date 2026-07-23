#include "jobs.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../cmd/pipeline.h"
#include "../options.h"
#include "shell.h"

#define MIN(x, y) (x) < (y) ? (x) : (y)

char *get_job_state_str(JobState js) {
    switch (js) {
    case JOB_STATE_FREE:
        return "Free";
    case JOB_STATE_FG:
        return "Running";
    case JOB_STATE_BG:
        return "Running";
    case JOB_STATE_STOPPED:
        return "Stopped";
    case JOB_STATE_DONE:
        return "Done";
    }
    return "";
}

int jt_init(JobTable *jt) {
    if (!jt) return -1;

    void *jobs = calloc(MAX_JOBS, sizeof(JobTableEntry));
    void *cmd_lines = malloc(MAX_JOBS * (MAX_CMD_LINE_LEN + 1) * sizeof(char));
    if (!jobs || !cmd_lines) {
        return -1;
    }

    jt->jobs = jobs;
    jt->cmd_lines = cmd_lines;

    return 0;
}

void free_jt(Shell *s) {
    if (!s) return;
    free(s->jt.jobs);
    free(s->jt.cmd_lines);
    s->jt.jobs = NULL;
    s->jt.cmd_lines = NULL;
}

int jt_first_free(Shell *s) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (s->jt.jobs[i].state == JOB_STATE_FREE) {
            return i;
        }
    }
    return -1;
}

int jt_add_entry(Shell *s, Pipeline *pl) {
    JobTableEntry jte = {
        .cmd_cnt = pl->cmd_cnt,
        .pgid = pl->pgid,
        .last_pid = pl->last_pid,
        .state = pl->run_in_bg ? JOB_STATE_BG : JOB_STATE_FG,
    };

    int first_free = jt_first_free(s);
    if (first_free < 0) {
        return -1;
    }

    s->jt.jobs[first_free] = jte;
    s->jt.most_recent_id = first_free;

    char *cmd_line = s->jt.cmd_lines + ((MAX_CMD_LINE_LEN + 1) * first_free);

    size_t line_len = MIN(strlen(pl->cmd_line), MAX_CMD_LINE_LEN);
    memcpy(cmd_line, pl->cmd_line, line_len);
    cmd_line[line_len] = '\0';

    return first_free;
}

inline bool job_id_is_valid(Shell *s, int job_id) {
    return job_id >= 0 && job_id < MAX_JOBS && s->jt.jobs[job_id].state != JOB_STATE_FREE;
}

int jt_get_job_id(Shell *s, pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (s->jt.jobs[i].pgid == pgid) {
            return i;
        }
    }
    return -1;
}

char *jt_get_cmd_line(Shell *s, int job_id) {
    if (!job_id_is_valid(s, job_id)) {
        return NULL;
    }

    return s->jt.cmd_lines + ((MAX_CMD_LINE_LEN + 1) * job_id);
}

bool jt_is_full(Shell *s) {
    return jt_first_free(s) < 0;
}

int jt_update_job_state(Shell *s, int job_id, JobState state) {
    if (!job_id_is_valid(s, job_id)) return -1;
    s->jt.jobs[job_id].state = state;
    return 0;
}

static void log_job(Shell *s, int job_id, char *status) {
    fprintf(stderr, "[%d] %s\t\t%s\n", job_id, status, jt_get_cmd_line(s, job_id));
}

int jt_update(Shell *s, int job_id, int cmd_status, bool ran_in_bg) {
    if (!job_id_is_valid(s, job_id)) return -1;

    int job_status;

    if (WIFSIGNALED(cmd_status)) {
        jt_update_job_state(s, job_id, JOB_STATE_DONE);

        if (WTERMSIG(cmd_status) == SIGKILL) {
            log_job(s, job_id, "Killed");
        } else {
            fprintf(stderr, "\n");
        }
        job_status = 128 + WTERMSIG(cmd_status);
    } else if (WIFSTOPPED(cmd_status)) {
        jt_update_job_state(s, job_id, JOB_STATE_STOPPED);
        fprintf(stderr, "\n");
        log_job(s, job_id, "Stopped");
        job_status = 128 + WSTOPSIG(cmd_status);
    } else {
        if (ran_in_bg) {
            log_job(s, job_id, "Done");
        }
        jt_update_job_state(s, job_id, JOB_STATE_DONE);
        job_status = WEXITSTATUS(cmd_status);
    }

    return job_status;
}

void jt_clear_finished_jobs(Shell *s) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (s->jt.jobs[i].state == JOB_STATE_DONE) {
            s->jt.jobs[i].state = JOB_STATE_FREE;
            if (i == s->jt.most_recent_id) {
                s->jt.most_recent_id = -1;
            }
        }
    }
    return;
}

void jt_print(Shell *s) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (s->jt.jobs[i].state != JOB_STATE_FREE) {
            printf("[%d] %s\t\t%s\n", i, get_job_state_str(s->jt.jobs[i].state), jt_get_cmd_line(s, i));
        }
    }
}