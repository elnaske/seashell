#pragma once

pid_t Fork();

int Execvp(const char *file, char *const *argv);

pid_t Waitpid(pid_t pid, int *stat_loc, int options);

int Tcsetpgrp(int fd, pid_t pgrp_id);

int Setpgid(pid_t pid, pid_t pgid);

int Kill(pid_t pid, int sig);

int Chdir(const char *path);

char *Getcwd(char *buf, size_t size);