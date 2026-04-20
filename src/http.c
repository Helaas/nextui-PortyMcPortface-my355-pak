#include "http.h"
#include "process.h"

#include <stdio.h>

int http_get_text(const char *url, char *buffer, size_t buffer_size) {
    char quoted_url[1024];
    char command[2048];

    if (shell_quote(quoted_url, sizeof(quoted_url), url) != 0)
        return -1;

    snprintf(command, sizeof(command),
        "curl -L --fail --silent --show-error "
        "-H 'Accept: application/vnd.github+json' "
        "-H 'User-Agent: porty-mcportface' %s",
        quoted_url);
    return run_capture_command(command, buffer, buffer_size);
}

int http_download_file(const char *url, const char *output_path) {
    char quoted_url[1024];
    char quoted_output[1024];
    char command[3072];

    if (shell_quote(quoted_url, sizeof(quoted_url), url) != 0)
        return -1;
    if (shell_quote(quoted_output, sizeof(quoted_output), output_path) != 0)
        return -1;

    snprintf(command, sizeof(command),
        "curl -L --fail --silent --show-error "
        "-H 'User-Agent: porty-mcportface' "
        "-o %s %s",
        quoted_output, quoted_url);
    return run_command(command);
}
