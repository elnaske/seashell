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
    if (strcmp(arg, "kill") == 0)
        return BUILTIN_KILL;

    return NOT_A_BUILTIN;
}

int builtin_cd(Shell *s, Command *cmd);
int builtin_fg_bg(Shell *s, Command *cmd, bool run_in_bg);
int builtin_jobs(Shell *s);
int builtin_kill(Shell *s, Command *cmd);

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
    case BUILTIN_KILL:
        return builtin_kill(s, cmd);
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
    if (apply_redirections(cmd) < 0) {
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

int convert_to_integer(char *s, int *int_out) {
    if (!int_out) return -1;

    char *end;

    long res = strtol(s, &end, 10);

    if (end == s || *end != '\0' || errno == ERANGE) {
        return -1;
    }

    *int_out = res;
    return 0;
}

int builtin_fg_bg(Shell *s, Command *cmd, bool run_in_bg) {
    if (cmd->argc > 2) {
        fprintf(stderr, "%s: too many arguments\n", cmd->argv[0]);
    }

    int new_state = run_in_bg ? JOB_STATE_BG : JOB_STATE_FG;

    int job_id = -1;
    if (cmd->argc == 1) {
        // TODO: resume most recent job instead of first job id
        for (int i = 0; i < MAX_JOBS; i++) {
            int curr_state = s->jt.jobs[i].state;
            if (curr_state == JOB_STATE_STOPPED || (!run_in_bg && curr_state == JOB_STATE_BG)) {
                job_id = i;
                break;
            }
        }

        if (job_id < 0) {
            fprintf(stderr, "%s: no current jobs\n", cmd->argv[0]);
            return 1;
        }
    } else {
        convert_to_integer(cmd->argv[1], &job_id);
    }

    if (!job_id_is_valid(s, job_id)) {
        fprintf(stderr, "%s: invalid job number\n", cmd->argv[0]);
        return 1;
    }
    if (run_in_bg && s->jt.jobs[job_id].state == JOB_STATE_BG) {
        fprintf(stderr, "bg: job %d already in background\n", job_id);
        return 1;
    }

    int pgid = s->jt.jobs[job_id].pgid;
    jt_update_job_state(s, job_id, new_state);

    printf("[%d] %s\n", job_id, jt_get_cmd_line(s, job_id));

    kill(-pgid, SIGCONT);

    if (!run_in_bg) {
        int cmd_cnt = s->jt.jobs[job_id].cmd_cnt;
        return await_job(s, job_id, pgid, cmd_cnt);
    }

    return 0;
}

int builtin_jobs(Shell *s) {
    jt_print(s);
    return 0;
}

int builtin_kill(Shell *s, Command *cmd) {
    if (cmd->argc != 3 || cmd->argv[1][0] != '-') {
        fprintf(stderr, "kill: usage: kill -signum [ pid | jobspec ]\n");
        return 1;
    }

    int sig;
    if (convert_to_integer(cmd->argv[1] + 1, &sig) < 0) { // skip '-'
        fprintf(stderr, "kill: invalid signal\n");
        return 1;
    }

    int pid;
    if (cmd->argv[2][0] == '%') {
        int job_id = -1;
        convert_to_integer(cmd->argv[2] + 1, &job_id); // skip '%'
        if (!job_id_is_valid(s, job_id)) {
            fprintf(stderr, "kill: invalid job id\n");
            return 1;
        }

        pid = -(s->jt.jobs[job_id].pgid);
    } else {
        if (convert_to_integer(cmd->argv[2], &pid) < 0) {
            fprintf(stderr, "kill: invalid process id\n");
            return 1;
        }
    }

    if (Kill(pid, sig) < 0) {
        return 1;
    }

    return 0;
}
