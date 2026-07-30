#pragma once
#include <stddef.h>

int tokenize_line(char *line, size_t len, char *line_tokenized, char ***tokens_out, size_t *cnt_out);