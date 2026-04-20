#ifndef PROCESS_H
#define PROCESS_H

#include <stddef.h>

int run_command(const char *command);
int run_capture_command(const char *command, char *buffer, size_t buffer_size);
int shell_quote(char *dst, size_t dst_size, const char *src);

#endif
