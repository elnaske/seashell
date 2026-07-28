#include "shell.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "../cmd/pipeline.h"
#include "../parser/parse.h"
#include "../sys/redirect.h"
#include "../sys/sighandlers.h"
#include "../sys/syscall_wrappers.h"
#include "../options.h"
#include "jobs.h"
#include "completions.h"

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

    if (!getcwd(s->cwd, MAX_PATHNAME_LENGTH)) {
        memcpy(s->cwd, "../", 4);
    }

    char **cmd_list = build_command_list();
    if (!cmd_list) {
        return -1;
    }
    s->completions = cmd_list;
    rl_attempted_completion_function = shell_completion;

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
    free(s->completions);
    s->completions = NULL;
}

static inline void set_last_status(Shell *s, int status) {
    s->last_status = (uint8_t)abs(status);
}

int shell_run(Shell *s) {
    const char* base_prompt = PROMPT_COL_1 "seashell" COL_CLR ":";
    
    const size_t buf_size = MAX_PATHNAME_LENGTH + 2 * strlen(base_prompt);
    char prompt_buf[buf_size];

    while (s->running) {
        snprintf(prompt_buf, buf_size, "%s" PROMPT_COL_2 "%s" COL_CLR "$ ", base_prompt, s->cwd);

        char *line = NULL;
        line = readline(prompt_buf);

        if (!line) {
            continue;
        }

        if (*line) {
            add_history(line);
        }

        Pipeline pl = {0};
        size_t len = strlen(line);
        int status = parse_line(s, line, len, &pl);

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

    free_shell(s);

    return s->last_status;
}