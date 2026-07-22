#include "builtins.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/shell.h"
#include "../sys/syscall_wrappers.h"
#include "commands.h"

inline bool is_builtin(BuiltinKind b) {
    return b != NOT_A_BUILTIN;
}

int match_builtin(Command *cmd) {
    char *arg = cmd->argv[0];

    if (strcmp(arg, "exit") == 0)
        return BUILTIN_EXIT;
    if (strcmp(arg, "cd") == 0)
        return BUILTIN_CD;
    if (strcmp(arg, "fg") == 0)
        return BUILTIN_FG;
    if (strcmp(arg, "bg") == 0)
        return BUILTIN_BG;
    if (strcmp(arg, "jobs") == 0)
        return BUILTIN_JOBS;

    return NOT_A_BUILTIN;
}

int builtin_cd(Shell *s, Command *cmd);
int builtin_fg_bg(Shell *s, Command *cmd, bool run_in_bg);
int builtin_jobs(Shell *s);

static int _run_builtin(Shell *s, BuiltinKind b, Command *cmd) {
    switch (b) {
    case BUILTIN_EXIT:
        s->running = false;
        return s->last_status;
    case BUILTIN_CD:
        return builtin_cd(s, cmd);
    case BUILTIN_FG:
        return builtin_fg_bg(s, cmd, false);
    case BUILTIN_BG:
        return builtin_fg_bg(s, cmd, true);
    case BUILTIN_JOBS:
        return builtin_jobs(s);
    case NOT_A_BUILTIN:
        return -1;
    }
    return -1;
}

int run_builtin(Shell *s, BuiltinKind b, Command *cmd) {
    if (b == NOT_A_BUILTIN)
        return -1;

    SavedFDs saved_fds;
    if (save_fds(&saved_fds) < 0) {
        return -1;
    }
    if (redirect_io(cmd) < 0) {
        restore_fds(&saved_fds);
        return -1;
    }

    int status = _run_builtin(s, b, cmd);

    if (restore_fds(&saved_fds) < 0) {
        return -1;
    }

    return status;
}

int builtin_cd(Shell *s, Command *cmd) {
    if (cmd->argc > 2) {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    char *dst = cmd->argc > 1 ? cmd->argv[1] : getenv("HOME");

    if (Chdir(dst) < 0) {
        return 1;
    }

    if (!Getcwd(s->cwd, 100) && errno == ERANGE) {
        memcpy(s->cwd, "../", 4);
    }

    return 0;
}

int builtin_fg_bg(Shell *s, Command *cmd, bool run_in_bg) {
    int new_state = run_in_bg ? JOB_STATE_BG : JOB_STATE_FG;

    int job_id = -1;
    if (cmd->argc == 1) {
        // TODO: resume most recent job instead of first job id
        for (int i = 0; i < MAX_JOBS; i++) {
            int curr_state = s->jt[i].state;
            if (curr_state == JOB_STATE_STOPPED || (!run_in_bg && curr_state == JOB_STATE_BG)) {
                job_id = i;
                break;
            }
            if (job_id < 0) {
                fprintf(stderr, "%s: no current jobs\n", cmd->argv[0]);
                return 1;
            }
        }
    } else {
        job_id = strtol(cmd->argv[1], NULL, 10);
    }

    if (!job_id_is_valid(s, job_id)) {
        fprintf(stderr, "%s: invalid job number\n", cmd->argv[0]);
        return 1;
    }
    if (run_in_bg && s->jt[job_id].state == JOB_STATE_BG) {
        fprintf(stderr, "bg: job %d already in background\n", job_id);
        return 1;
    }

    int pgid = s->jt[job_id].pgid;
    jt_update_job_state(s, job_id, new_state);

    printf("[%d] %d", job_id, pgid);

    kill(-pgid, SIGCONT);

    if (!run_in_bg) {
        int cmd_cnt = s->jt[job_id].cmd_cnt;
        return await_job(s, job_id, pgid, cmd_cnt);
    }

    return 0;
}

int builtin_jobs(Shell *s) {
    jt_print(s);
    return 0;
}
