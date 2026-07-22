#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <wait.h>

typedef struct Shell Shell;
typedef struct Pipeline Pipeline;

typedef enum JobState {
    JOB_STATE_FREE,
    JOB_STATE_FG,
    JOB_STATE_BG,
    JOB_STATE_STOPPED,
    JOB_STATE_DONE,
} JobState;

typedef struct JobTableEntry {
    size_t cmd_cnt;
    pid_t pgid;
    pid_t last_pid;
    JobState state;
} JobTableEntry;

typedef JobTableEntry *JobTable;

int jt_add_entry(Shell *s, Pipeline *pl);

bool job_id_is_valid(Shell *s, int job_id);

bool jt_is_full(Shell *s);

int jt_update_job_state(Shell *s, int job_id, JobState state);

int jt_get_job_id(Shell *s, pid_t pgid);

void jt_clear_finished_jobs(Shell *s);

void jt_print(Shell *s);