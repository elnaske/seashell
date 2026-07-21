#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <wait.h>

#include "options.h"

typedef struct Job Job;

typedef enum JobState {
    JOB_STATE_FREE,
    JOB_STATE_FG,
    JOB_STATE_BG,
    JOB_STATE_STOPPED,
    JOB_STATE_DONE,
} JobState;

typedef struct JobTableEntry {
    // char *cmd_line; TODO: add this
    size_t cmd_cnt;
    pid_t pgid;
    pid_t last_pid;
    JobState state;
} JobTableEntry;

typedef struct Shell {
    JobTableEntry *job_table;
    pid_t pgid;
    pid_t fg_pgid;
    uint8_t last_status;
    char cwd[MAX_PATHNAME_LENGTH];
    bool running;
} Shell;

int shell_init(Shell *s);

int add_job_table_entry(Shell *s, Job *job);

bool is_job_id_valid(Shell *s, int job_id);

bool job_table_is_full(Shell *s);

int job_table_update_state(Shell *s, int job_id, int status);

int job_table_mark_finished(Shell *s, pid_t pid);

void job_table_print(Shell *s);

int shell_run(Shell *s);