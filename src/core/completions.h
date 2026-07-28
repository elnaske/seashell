#pragma once

void free_command_list();

char **build_command_list();

char **shell_completion(const char *text, int start, int end);