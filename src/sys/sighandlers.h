#pragma once

typedef struct Shell Shell;

void install_signal_handler(int signum, void (*handler)(int));

void reap_children(Shell *s);

void sigchld_handler(int sig);

void keyboard_interrupt_handler(int sig);