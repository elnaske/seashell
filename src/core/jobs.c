#include "jobs.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../cmd/commands.h"
#include "../options.h"
#include "shell.h"

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

int jt_add_entry(Shell *s, Pipeline *pl) {
    JobTableEntry jte = {
        .cmd_cnt = pl->cmd_cnt,
        .pgid = pl->pgid,
        .last_pid = pl->last_pid,
        .state = pl->run_in_bg ? JOB_STATE_BG : JOB_STATE_FG,
    };

    for (size_t i = 0; i < MAX_JOBS; i++) {
        if (s->jt[i].state == JOB_STATE_FREE) {
            s->jt[i] = jte;
            return i;
        }
    }
    return -1;
}

inline bool job_id_is_valid(Shell *s, int job_id) {
    return job_id >= 0 && job_id < MAX_JOBS && s->jt[job_id].state != JOB_STATE_FREE;
}

bool jt_is_full(Shell *s) {
    for (size_t i = 0; i < MAX_JOBS; i++) {
        if (s->jt[i].state == JOB_STATE_FREE) {
            return false;
        }
    }
    return true;
}

int jt_update_job_state(Shell *s, int job_id, JobState state) {
    if (!job_id_is_valid(s, job_id)) return -1;
    s->jt[job_id].state = state;
    return 0;
}

int jt_get_job_id(Shell *s, pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (s->jt[i].pgid == pgid) {
            return i;
        }
    }
    return -1;
}

void jt_clear_finished_jobs(Shell *s) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (s->jt[i].state == JOB_STATE_DONE) {
            s->jt[i].state = JOB_STATE_FREE;
        }
    }
    return;
}

void jt_print(Shell *s) {
    for (int i = 0; i < MAX_JOBS; i++) {
        JobTableEntry jte = s->jt[i];
        if (jte.state != JOB_STATE_FREE) {
            printf("[%d] %s\t\t%d\n", i, get_job_state_str(jte.state), jte.pgid);
        }
    }
}