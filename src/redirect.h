#pragma once

typedef struct Command Command;

typedef struct {
    int stdin;
    int stdout;
    int stderr;
} SavedFDs;

int save_fds(SavedFDs *fd_out);

int restore_fds(SavedFDs *saved);

int redirect_io(Command *cmd);