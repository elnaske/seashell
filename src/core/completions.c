#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <readline/readline.h>

#include "../cmd/builtins.h"
#include "shell.h"

extern const Builtin g_builtins[];
extern Shell shell;

static void add_builtins(char **cmd_list) {
    size_t idx = 0;
    for (; g_builtins[idx].strname != NULL; idx++) {
        cmd_list[idx] = g_builtins[idx].strname;
    }
    cmd_list[idx] = NULL;
}

char **build_command_list() {
    // TODO: determine malloc size
    char **cmd_list = malloc(7 * sizeof(char *));
    if (!cmd_list) {
        return NULL;
    }

    add_builtins(cmd_list);

    return cmd_list;
}

static inline bool starts_with(const char *s, const char *prefix, size_t prefix_len) {
    return strncmp(s, prefix, prefix_len) == 0;
}

char *command_generator(const char *text, int state) {
    static size_t idx;
    static size_t prefix_len;

    if (state == 0) {
        idx = 0;
        prefix_len = strlen(text);
    }

    while (shell.completions[idx] != NULL) {
        char *cmd = shell.completions[idx++];
        if (starts_with(cmd, text, prefix_len)) {
            return strdup(cmd);
        }
    }

    return NULL;
}

char **shell_completion(const char *text, int start, int end) {
    (void)end;

    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }
    
    return NULL;
}