#include "tokenize.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "../options.h"

typedef enum {
    NORMAL,
    IN_SINGLE_QUOTES,
    IN_DOUBLE_QUOTES,
} TokenizerState;

int tokenize_line(char *line, char *line_tokenized, char ***tokens_out, size_t *cnt_out) {
    if (!line || !line_tokenized || !tokens_out) return -1;

    char **tokens = malloc(sizeof(char *) * (MAX_ARGS + 1)); // terminated by NULL ptr
    if (!tokens) return -1;

    size_t token_cnt = 0;
    TokenizerState state = NORMAL;

    char *curr = line;
    char *token_start = line_tokenized;
    size_t tok_idx = 0;
    for (; *curr != '\0'; curr++) {
        switch (state) {
        case NORMAL:
            if (isspace(*curr)) {
                if (tok_idx > 0 && line_tokenized[tok_idx - 1] != '\0') {
                    line_tokenized[tok_idx++] = '\0';
                    tokens[token_cnt++] = token_start;
                    token_start = line_tokenized + tok_idx;
                }
            } else if (*curr == '\'') {
                state = IN_SINGLE_QUOTES;
            } else if (*curr == '"') {
                state = IN_DOUBLE_QUOTES;
            } else {
                line_tokenized[tok_idx++] = *curr;
            }
            break;
        case IN_SINGLE_QUOTES:
            if (*curr == '\'') {
                state = NORMAL;
            } else {
                line_tokenized[tok_idx++] = *curr;
            }
            break;
        case IN_DOUBLE_QUOTES:
            if (*curr == '"') {
                state = NORMAL;
            } else {
                line_tokenized[tok_idx++] = *curr;
            }
            break;
        }

        if (token_cnt >= MAX_ARGS) {
            fprintf(stderr, "Shell warning: Max number of arguments reached; ignoring all after '%s'\n", tokens[token_cnt - 1]);
            break;
        }
    }

    if (state != NORMAL) {
        return PARSE_ERR_UNMATCHED_QUOTE;
    }

    if (tok_idx > 0 && *curr == '\0') {
        line_tokenized[tok_idx++] = '\0';
        tokens[token_cnt++] = token_start;
    }

    if (cnt_out) {
        *cnt_out = token_cnt;
    }

    tokens[token_cnt] = NULL;
    *tokens_out = tokens;

    return PARSE_OK;
}