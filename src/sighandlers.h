#pragma once

void install_signal_handler(int signum, void (*handler)(int));

void reap_children();

void sigchld_handler(int sig);

void keyboard_interrupt_handler(int sig);