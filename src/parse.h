#pragma once
#include <stddef.h>

typedef struct Job Job;

typedef enum {
    PARSE_OK,
    PARSE_ERR_MALLOC,
    PARSE_ERR_FILENAME,
    PARSE_ERR_LEADING_PIPE,
    PARSE_ERR_DANGLING_PIPE,
} ParseStatus;

void parse_error(int status);

int parse_line(char *line, size_t len, Job *job_out);