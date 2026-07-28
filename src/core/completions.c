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

#define MAX_CMDS 8192

static void add_builtins(char **cmd_list, size_t *start) {
    size_t list_idx = *start;
    for (size_t i = 0; g_builtins[i].strname; i++) {
        cmd_list[list_idx++] = strdup(g_builtins[i].strname);
    }
    *start = list_idx;
}

static inline bool is_executable(char *filepath) {
    return access(filepath, X_OK) == 0;
}

static void add_dir_cmds(char **cmd_list, size_t *start, DIR *dp, char *dir_name) {
    struct dirent *entry;
    size_t list_idx = *start;

    while ((entry = readdir(dp)) != NULL && list_idx < MAX_CMDS) {
        size_t buf_size = strlen(dir_name) + 1 + strlen(entry->d_name) + 1;
        char filepath_buf[buf_size];
        snprintf(filepath_buf, buf_size, "%s/%s", dir_name, entry->d_name);

        if (is_executable(filepath_buf)) {
            cmd_list[list_idx++] = strdup(entry->d_name);
        }
    }
    *start = list_idx;
}

static void add_path_cmds(char **cmd_list, size_t *start) {
    char *path = getenv("PATH");
    if (!path) {
        return;
    }

    char *path_cpy = strdup(path);

    char *next_dir = strtok(path_cpy, ":");
    while (next_dir) {
        // WSL: skip windows
        if (strncmp(next_dir, "/mnt/c/", 7) == 0) {
            next_dir = strtok(NULL, ":");
            continue;
        }

        DIR *dp = opendir(next_dir);

        if (dp) {
            add_dir_cmds(cmd_list, start, dp, next_dir);

            closedir(dp);
        }

        if (*start >= MAX_CMDS) {
            break;
        }

        next_dir = strtok(NULL, ":");
    }

    free(path_cpy);
}

void free_command_list(char **cmd_list) {
    for (size_t i = 0; cmd_list[i] != NULL; i++) {
        free(cmd_list[i]);
    }
    free(cmd_list);
}

char **build_command_list() {
    // TODO: dynamic array instead of MAX_CMDS
    char **cmd_list = malloc((MAX_CMDS + 1) * sizeof(char *));
    if (!cmd_list) {
        return NULL;
    }

    size_t len = 0;

    add_builtins(cmd_list, &len);
    add_path_cmds(cmd_list, &len);

    cmd_list[len] = NULL;

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
        return rl_completion_matches(text, command_generator);
    }

    return NULL;
}