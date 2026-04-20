#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

int http_get_text(const char *url, char *buffer, size_t buffer_size);
int http_download_file(const char *url, const char *output_path);

#endif
