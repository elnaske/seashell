#include "shell.h"

#include <limits.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../cmd/pipeline.h"
#include "../options.h"
#include "../parser/parse.h"
#include "../sys/redirect.h"
#include "../sys/sighandlers.h"
#include "../sys/syscall_wrappers.h"
#include "completions.h"
#include "job_table.h"

#ifdef COLORED_PROMPT
#define COL_CLR "\033[0m"
#else
#define PROMPT_COL_1 ""
#define PROMPT_COL_2 ""
#define COL_CLR ""
#endif

extern volatile sig_atomic_t sigchld_received;

int shell_init(Shell *s) {
    JobTable jt;
    if (jt_init(&jt) < 0) {
        return -1;
    }

    s->jt = jt;
    s->pgid = getpgrp();
    s->fg_pgid = -1;
    s->last_status = 0;

    if (!getcwd(s->cwd, PATH_MAX)) {
        memcpy(s->cwd, "../", 4);
    }

    char **cmd_list = build_command_list();
    if (!cmd_list) {
        return -1;
    }
    s->cmd_list = cmd_list;
    rl_attempted_completion_function = &shell_completion;

    install_signal_handler(SIGCHLD, &sigchld_handler);
    install_signal_handler(SIGINT, &keyboard_interrupt_handler);
    install_signal_handler(SIGTSTP, &keyboard_interrupt_handler);
    install_signal_handler(SIGTTOU, SIG_IGN);
    install_signal_handler(SIGTTIN, SIG_IGN);

    s->running = true;

    return 0;
}

void free_shell(Shell *s) {
    free_jt(s);
    free_command_list(s->cmd_list);
}

static inline void update_prompt(Shell *s, char *buf, size_t buf_size) {
    snprintf(
        buf,
        buf_size, 
        PROMPT_COL_1 "seashell" COL_CLR ":" 
        PROMPT_COL_2 "%s" COL_CLR "$ ",
        s->cwd
    );
}

static inline void set_last_status(Shell *s, int status) {
    s->last_status = (uint8_t)abs(status);
}

int shell_run(Shell *s) {
    bool history_enabled = getenv("SEASHELL_HISTORY_DISABLED") == NULL;
    if (history_enabled) {
        read_history(HISTORY_FILENAME);
    }

    size_t buf_size = PATH_MAX + 64;
    char prompt_buf[buf_size];

    while (s->running) {
        update_prompt(s, prompt_buf, buf_size);

        char *line = NULL;
        line = readline(prompt_buf);

        if (!line) {
            continue;
        }

        if (*line) {
            add_history(line);
        }

        Pipeline pl = {0};
        int status = parse_line(s, line, strlen(line), &pl);

        if (status == PARSE_OK) {
            status = run_pipeline(s, &pl);
        } else {
            print_syntax_error(status);
        }

        set_last_status(s, status);

        if (sigchld_received) {
            reap_children(s);
            sigchld_received = 0;
        }

        jt_clear_finished_jobs(s);

        free_pipeline(&pl);
        free(line);
    }

    if (history_enabled) {
        stifle_history(HISTORY_MAX_LEN);
        write_history(HISTORY_FILENAME);
        history_truncate_file(HISTORY_FILENAME, HISTORY_MAX_LEN);
    }

    free_shell(s);

    return s->last_status;
}