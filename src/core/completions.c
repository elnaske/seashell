#include <dirent.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../cmd/builtins.h"
#include "shell.h"

extern const Builtin g_builtins[];
extern Shell shell;

// Dynamic array for caching commands for autocompletion
typedef struct {
    char **entries;
    size_t len;
    size_t cap;
} CmdList;

static inline bool is_executable(char *filepath) {
    return access(filepath, X_OK) == 0;
}

static inline bool starts_with(const char *s, const char *prefix, size_t prefix_len) {
    return strncmp(s, prefix, prefix_len) == 0;
}

static inline int cmd_list_add(CmdList *cmd_list, char *cmd) {
    if (cmd_list->len >= cmd_list->cap - 1) { // accounting for NULL terminator
        size_t new_cap = cmd_list->cap * 2;
        if (new_cap <= cmd_list->cap) {
            return -1;
        }

        char **tmp = realloc(cmd_list->entries, new_cap * sizeof(char *));
        if (!tmp) {
            return -1;
        }

        cmd_list->entries = tmp;
        cmd_list->cap = new_cap;
    }

    cmd_list->entries[cmd_list->len++] = strdup(cmd);
    return 0;
}

int add_builtins(CmdList *cmd_list) {
    for (size_t i = 0; g_builtins[i].strname; i++) {
        if (cmd_list_add(cmd_list, g_builtins[i].strname) < 0) {
            return -1;
        }
    }
    return 0;
}

int add_dir_cmds(CmdList *cmd_list, DIR *dp, char *dir_name) {
    struct dirent *entry;

    while ((entry = readdir(dp)) != NULL) {
        size_t buf_size = strlen(dir_name) + 1 + strlen(entry->d_name) + 1;
        char filepath_buf[buf_size];
        snprintf(filepath_buf, buf_size, "%s/%s", dir_name, entry->d_name);

        if (is_executable(filepath_buf)) {
            if (cmd_list_add(cmd_list, entry->d_name) < 0) {
                return -1;
            }
        }
    }

    return 0;
}

int add_path_cmds(CmdList *cmd_list) {
    char *path = getenv("PATH");
    if (!path) {
        return -1;
    }

    char *path_cpy = strdup(path);

    char *next_dir = strtok(path_cpy, ":");
    while (next_dir) {
        // WSL: skip windows mount point
        if (!starts_with(next_dir, "/mnt/", 5)) {
            DIR *dp = opendir(next_dir);

            if (dp) {
                int status = add_dir_cmds(cmd_list, dp, next_dir);

                closedir(dp);

                if (status < 0) {
                    free(path_cpy);
                    return -1;
                }
            }
        }

        next_dir = strtok(NULL, ":");
    }

    free(path_cpy);

    return 0;
}

void free_command_list(char **cmd_list) {
    for (size_t i = 0; cmd_list[i] != NULL; i++) {
        free(cmd_list[i]);
    }
    free(cmd_list);
}

char **build_command_list() {
    CmdList cmd_list = {
        .entries = NULL,
        .len = 0,
        .cap = 1024,
    };

    char **entries = malloc(cmd_list.cap * sizeof(char *));
    if (!entries) {
        return NULL;
    }

    cmd_list.entries = entries;

    add_builtins(&cmd_list);
    add_path_cmds(&cmd_list);

    cmd_list.entries[cmd_list.len] = NULL;

    return cmd_list.entries;
}

char *command_generator(const char *text, int state) {
    static size_t idx;
    static size_t prefix_len;

    if (state == 0) {
        idx = 0;
        prefix_len = strlen(text);
    }

    while (shell.cmd_list[idx] != NULL) {
        char *cmd = shell.cmd_list[idx++];
        if (starts_with(cmd, text, prefix_len)) {
            return strdup(cmd);
        }
    }

    return NULL;
}

char **shell_completion(const char *text, int start, int end) {
    (void)end;

    if (start == 0) {
        return rl_completion_matches(text, &command_generator);
    }

    return NULL;
}