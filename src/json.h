#ifndef JSON_H
#define JSON_H

#include <stddef.h>

#define MAX_REMOTE_RELEASES 10

typedef struct {
    char runtime_source[32];
    char runtime_version[64];
    char runtime_asset_name[128];
    char runtime_asset_url[1024];
    char published_at[32];
    long size_bytes;
} remote_metadata;

typedef struct {
    char installed_runtime_source[32];
    char installed_runtime_version[64];
    char installed_runtime_asset_name[128];
    char installed_runtime_asset_url[1024];
    char installed_at[32];
    char last_checked_at[32];
} install_state;

int parse_release_candidates(const char *releases_json, const char *asset_name,
    remote_metadata *out, size_t out_capacity, size_t *out_count);
int parse_install_state(const char *manifest_json, install_state *out);
int format_install_state_json(const install_state *state, char *buffer, size_t buffer_size);

#endif
