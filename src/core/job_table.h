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
    size_t cmd_cnt; // Number of commands in the pipeline
    pid_t pgid;
    pid_t last_pid; // pid of the last command in the pipeline. Waiting on it ensures that _all_ commands are done.
    JobState state;
} JobTableEntry;

typedef struct JobTable {
    JobTableEntry *jobs;
    char *cmd_lines;
    int most_recent_id; // only storing the most recent for now (could be replaced with a linked list in the future)
} JobTable;

int jt_init(JobTable *jt);

void free_jt(Shell *s);

int jt_add_entry(Shell *s, Pipeline *pl);

bool job_id_is_valid(Shell *s, int job_id);

int jt_first_free(Shell *s);

bool jt_is_full(Shell *s);

int jt_update_job_state(Shell *s, int job_id, JobState state);

int jt_update(Shell *s, int job_id, int cmd_status, bool ran_in_bg);

int jt_get_job_id(Shell *s, pid_t pgid);

char *jt_get_cmd_line(Shell *s, int job_id);

void jt_clear_finished_jobs(Shell *s);

void jt_print(Shell *s);