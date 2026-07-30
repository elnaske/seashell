#include "tokenize.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../options.h"
#include "parse.h"

typedef enum {
    NORMAL,
    IN_SINGLE_QUOTES,
    IN_DOUBLE_QUOTES,
} TokenizerState;

static bool is_escapable(char c) {
    return c == '\'' || c == '"';
}

int tokenize_line(char *line, size_t len, char *line_tokenized, char ***tokens_out, size_t *cnt_out) {
    if (!line || !line_tokenized || !tokens_out) return -1;

    char **tokens = malloc(sizeof(char *) * (MAX_ARGS + 1)); // terminated by NULL ptr
    if (!tokens) return -1;

    size_t token_cnt = 0;
    TokenizerState state = NORMAL;

    bool building_token;

    char *token_start = line_tokenized;
    size_t tok_idx = 0;

    size_t line_idx = 0;
    while (line_idx < len) {
        char c = line[line_idx];
        switch (state) {
        case NORMAL:
            if (isspace(c)) {
                if (building_token) {
                    line_tokenized[tok_idx++] = '\0';
                    tokens[token_cnt++] = token_start;
                    token_start = line_tokenized + tok_idx;
                    building_token = false;
                }
            } else if (c == '\\' && line_idx + 1 < len && is_escapable(line[line_idx + 1])) {
                line_tokenized[tok_idx++] = line[++line_idx];
                building_token = true;
            } else if (c == '\'') {
                state = IN_SINGLE_QUOTES;
                building_token = true;
            } else if (c == '"') {
                state = IN_DOUBLE_QUOTES;
                building_token = true;
            } else {
                line_tokenized[tok_idx++] = c;
                building_token = true;
            }
            break;
        case IN_SINGLE_QUOTES:
            if (c == '\\' && line_idx + 1 < len && line[line_idx + 1] == '\'') {
                line_tokenized[tok_idx++] = line[++line_idx];
            } else if (c == '\'') {
                state = NORMAL;
            } else {
                line_tokenized[tok_idx++] = c;
            }
            break;
        case IN_DOUBLE_QUOTES:
            if (c == '\\' && line_idx + 1 < len && line[line_idx + 1] == '"') {
                line_tokenized[tok_idx++] = line[++line_idx];
            } else if (c == '"') {
                state = NORMAL;
            } else {
                line_tokenized[tok_idx++] = c;
            }
            break;
        }

        if (token_cnt >= MAX_ARGS) {
            fprintf(stderr, "Shell warning: Max number of arguments reached; ignoring all after '%s'\n", tokens[token_cnt - 1]);
            break;
        }

        line_idx++;
    }

    if (state != NORMAL) {
        return PARSE_ERR_UNMATCHED_QUOTE;
    }

    if (building_token) {
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