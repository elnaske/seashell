#include "tokenize.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../options.h"

char **tokenize_line(char *line, size_t *cnt_out) {
    if (!line) return NULL;

    char **tokens = malloc(sizeof(char *) * (MAX_ARGS + 1)); // terminated by NULL ptr
    if (!tokens) return NULL;

    size_t token_cnt = 0;

    char *curr = line;
    while (*curr) {
        while (isspace(*curr)) {
            curr++;
        }

        if (*curr == '\0') {
            break;
        }

        char *token_start = curr;

        while (*curr && !isspace(*curr)) {
            curr++;
        }

        if (*curr) {
            *curr = '\0';
            curr++;
        }

        tokens[token_cnt++] = token_start;

        if (token_cnt >= MAX_ARGS) {
            fprintf(stderr, "Shell warning: Max number of arguments exceeded; ignoring all after '%s'\n", tokens[token_cnt - 1]);
            break;
        }
    }

    if (cnt_out) {
        *cnt_out = token_cnt;
    }

    tokens[token_cnt] = NULL;

    return tokens;
}