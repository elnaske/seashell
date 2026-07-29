#pragma once
#include <stdbool.h>
#include <limits.h>

#include "../options.h"
#include "jobs.h"

typedef struct Pipeline Pipeline;

typedef struct Shell {
    JobTable jt;
    pid_t pgid;
    volatile pid_t fg_pgid;
    uint8_t last_status;
    char cwd[PATH_MAX];
    char **cmd_list;
    bool running;
} Shell;

int shell_init(Shell *s);

int shell_run(Shell *s);