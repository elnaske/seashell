#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <wait.h>

#include "options.h"

typedef struct Shell {
    pid_t pgid;
    pid_t fg_pgid;
    uint8_t last_status;
    char cwd[MAX_PATHNAME_LENGTH];
    bool running;
} Shell;

int shell_init(Shell *s);

int shell_run(Shell *s);