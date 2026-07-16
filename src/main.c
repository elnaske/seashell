#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "parse.h"
#include "redirect.h"
#include "shell.h"
#include "sighandlers.h"
#include "syscall_wrappers.h"

Shell shell = {0}; // needs to be accessible by sighandlers, hence a global variable

int main() {
    shell_init(&shell);

    shell_run(&shell);

    return 0;
}