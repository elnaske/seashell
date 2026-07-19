#pragma once
#include <stdbool.h>
#include <wait.h>

#include "options.h"

typedef struct Shell {
    pid_t pgid;
    pid_t fg_pgid;
    char cwd[MAX_PATHNAME_LENGTH];
    bool running;
} Shell;

int shell_init(Shell *s);

int shell_run(Shell *s);