#include <wait.h>
#pragma once

#define MAX_PATHNAME_LENGTH 100

typedef struct {
    pid_t pgid;
    pid_t fg_pgid;
    char cwd[MAX_PATHNAME_LENGTH];
} Shell;

int shell_init(Shell *s);

int shell_run(Shell *s);