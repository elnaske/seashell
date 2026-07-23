#include "commands.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

#include "../core/shell.h"
#include "../options.h"
#include "../sys/syscall_wrappers.h"
#include "builtins.h"
#include "pipeline.h"

void cmd_add_redirection(Command *cmd, char *file, int fd, int o_flag) {
    if (cmd->n_redirects >= MAX_REDIRECTS) {
        fprintf(stderr, "Shell warning: Max number of redirects exceeded; ignoring all after '%s'\n", cmd->redirects[cmd->n_redirects - 1].file);
        return;
    }

    Redirect redir = {.file = file, .fd = fd, .o_flag = o_flag};
    cmd->redirects[cmd->n_redirects++] = redir;
}

int run_command(Shell *s, Command *cmd, Pipeline *pl, bool is_last) {
    if (!cmd || !cmd->argc || !pl) return -1;

    int pipefd[2] = {-1, -1};
    if (!is_last && Pipe(pipefd) < 0) {
        if (pl->prev_pipe > -1) {
            Close(pl->prev_pipe);
        }
        return -1;
    }

    int pipe_write = pipefd[0];
    int pipe_read = pipefd[1];

    pid_t pid = Fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        Setpgid(0, pl->pgid); // pgid = 0 for first command

        if (apply_pipe(pl->prev_pipe, pipe_read, pipe_write) < 0) {
            exit(1);
        }
        if (apply_redirections(cmd) < 0) {
            exit(1);
        }

        BuiltinKind b = match_builtin(cmd);
        if (is_builtin(b)) {
            int status = run_builtin(s, b, &cmd);
            exit(status);

        } else {
            Execvp(cmd->argv[0], cmd->argv);
        }
    }

    if (pl->pgid == 0) {
        pl->pgid = pid;
    }

    Setpgid(pid, pl->pgid);

    if (update_pipe_read_end(pl, pipe_read, pipe_write) < 0) {
        return -1;
    }

    pl->last_pid = pid;

    return 0;
}