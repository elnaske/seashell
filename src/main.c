#define _GNU_SOURCE

#include "shell.h"

Shell shell = {0}; // needs to be accessible by sighandlers, hence a global variable

int main() {
    if (shell_init(&shell) < 0) {
        return 1;
    }

    return shell_run(&shell);
}