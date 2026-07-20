#define _GNU_SOURCE

#include "shell.h"

Shell shell = {0}; // needs to be accessible by sighandlers, hence a global variable

int main() {
    shell_init(&shell);

    return shell_run(&shell);
}