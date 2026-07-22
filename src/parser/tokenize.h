#pragma once
#include <stddef.h>

char **tokenize_line(char *line, size_t len, size_t *cnt_out);