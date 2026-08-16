#pragma once
#include <linux/limits.h>

#define COLORED_PROMPT
#ifdef COLORED_PROMPT
    #define PROMPT_COL_1 "\033[32m" // Green
    #define PROMPT_COL_2 "\033[34m" // Blue
#endif

#define MAX_ARGS 16
#define MAX_JOBS 32
#define MAX_CMDS_PER_JOB 5
#define MAX_CMD_LINE_LEN 64
#define MAX_REDIRECTS 3

#define HISTORY_FILENAME ".seashell_history"
#define HISTORY_MAX_LEN 1000