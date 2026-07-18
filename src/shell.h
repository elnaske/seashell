#pragma once
#include <wait.h>
#include "options.h"

typedef struct Shell {
    pid_t pgid;
    pid_t fg_pgid;
    char cwd[MAX_PATHNAME_LENGTH];
} Shell;

int shell_init(Shell *s);

int shell_run(Shell *s);