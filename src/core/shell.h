#pragma once
#include <stdbool.h>
#include <limits.h>

#include "../options.h"
#include "job_table.h"

typedef struct Pipeline Pipeline;

typedef struct Shell {
    JobTable jt;
    char **cmd_list;        // cached list of commans for autocompletion
    pid_t pgid;
    volatile pid_t fg_pgid;
    uint8_t last_status;    // exit code of the previous command
    char cwd[PATH_MAX];
    bool running;
} Shell;

int shell_init(Shell *s);

int shell_run(Shell *s);