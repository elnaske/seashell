#pragma once

void install_signal_handler(int signum, void (*handler)(int));

void reap_children(int sig);

void sigint_handler(int sig);