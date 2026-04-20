#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int run_command(const char *command) {
    int rc = system(command);

    if (rc == -1)
        return -1;
    if (WIFEXITED(rc))
        return WEXITSTATUS(rc);
    return -1;
}

int run_capture_command(const char *command, char *buffer, size_t buffer_size) {
    FILE *pipe;
    size_t total = 0;
    int status;
    int truncated = 0;

    if (buffer == NULL || buffer_size == 0)
        return -1;

    buffer[0] = '\0';
    pipe = popen(command, "r");
    if (pipe == NULL)
        return -1;

    while (!feof(pipe)) {
        if (!truncated && total + 1 < buffer_size) {
            size_t read_now = fread(buffer + total, 1, buffer_size - total - 1, pipe);
            total += read_now;
            if (read_now == 0)
                break;
            if (total + 1 >= buffer_size)
                truncated = 1;
        } else {
            char discard[4096];
            size_t read_now = fread(discard, 1, sizeof(discard), pipe);

            if (read_now == 0)
                break;
            truncated = 1;
        }
    }
    buffer[total] = '\0';

    status = pclose(pipe);
    if (status == -1)
        return -1;
    if (truncated)
        return -1;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

int shell_quote(char *dst, size_t dst_size, const char *src) {
    size_t i;
    size_t used = 0;

    if (dst == NULL || dst_size < 3 || src == NULL)
        return -1;

    dst[used++] = '\'';
    for (i = 0; src[i] != '\0'; i++) {
        if (src[i] == '\'') {
            if (used + 4 >= dst_size)
                return -1;
            memcpy(dst + used, "'\\''", 4);
            used += 4;
        } else {
            if (used + 1 >= dst_size)
                return -1;
            dst[used++] = src[i];
        }
    }
    if (used + 2 > dst_size)
        return -1;
    dst[used++] = '\'';
    dst[used] = '\0';
    return 0;
}
