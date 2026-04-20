#include "json.h"

#include "../third_party/jsmn/jsmn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_JSON_TOKENS 65536

static int token_equals(const char *json, const jsmntok_t *token, const char *text) {
    size_t len = strlen(text);
    return token->type == JSMN_STRING &&
        (size_t)(token->end - token->start) == len &&
        strncmp(json + token->start, text, len) == 0;
}

static int token_copy(char *dst, size_t dst_size, const char *json, const jsmntok_t *token) {
    size_t len;

    if (dst == NULL || dst_size == 0 || json == NULL || token == NULL)
        return -1;

    len = (size_t)(token->end - token->start);
    if (len + 1 > dst_size)
        return -1;

    memcpy(dst, json + token->start, len);
    dst[len] = '\0';
    return 0;
}

static int skip_token(const jsmntok_t *tokens, int index) {
    int count = 1;

    while (count > 0) {
        int type = tokens[index].type;
        int size = tokens[index].size;

        index++;
        count--;

        if (type == JSMN_OBJECT) {
            count += size * 2;
        } else if (type == JSMN_ARRAY) {
            count += size;
        }
    }

    return index;
}

static int find_object_value(const char *json, const jsmntok_t *tokens, int object_index, const char *key) {
    int index = object_index + 1;
    int remaining = tokens[object_index].size;

    while (remaining-- > 0) {
        int value_index = index + 1;

        if (token_equals(json, &tokens[index], key))
            return value_index;

        index = skip_token(tokens, value_index);
    }

    return -1;
}

static int parse_json(const char *json, jsmntok_t *tokens, size_t capacity) {
    jsmn_parser parser;
    int count;

    if (json == NULL || tokens == NULL || capacity == 0)
        return -1;

    jsmn_init(&parser);
    count = jsmn_parse(&parser, json, strlen(json), tokens, capacity);
    if (count < 0)
        return -1;

    return count;
}

static long parse_long_token(const char *json, const jsmntok_t *token) {
    char buffer[32];

    if (token_copy(buffer, sizeof(buffer), json, token) != 0)
        return -1;

    return strtol(buffer, NULL, 10);
}

static int copy_release_asset(const char *json, const jsmntok_t *tokens, int object_index,
    const char *asset_name, remote_metadata *out) {
    int name_index;
    int url_index;
    int size_index;
    char name_buffer[128];

    name_index = find_object_value(json, tokens, object_index, "name");
    if (name_index < 0 || token_copy(name_buffer, sizeof(name_buffer), json, &tokens[name_index]) != 0)
        return 0;

    if (strcmp(name_buffer, asset_name) != 0)
        return 0;

    url_index = find_object_value(json, tokens, object_index, "browser_download_url");
    size_index = find_object_value(json, tokens, object_index, "size");
    if (url_index < 0 || size_index < 0)
        return -1;

    if (token_copy(out->runtime_asset_name, sizeof(out->runtime_asset_name), json, &tokens[name_index]) != 0 ||
        token_copy(out->runtime_asset_url, sizeof(out->runtime_asset_url), json, &tokens[url_index]) != 0)
        return -1;

    out->size_bytes = parse_long_token(json, &tokens[size_index]);
    if (out->size_bytes < 0)
        return -1;

    return 1;
}

static int token_is_false(const char *json, const jsmntok_t *token) {
    size_t len = strlen("false");

    return token->type == JSMN_PRIMITIVE &&
        (size_t)(token->end - token->start) == len &&
        strncmp(json + token->start, "false", len) == 0;
}

static int parse_release_object(const char *release_json, const jsmntok_t *tokens, int object_index,
    const char *asset_name, remote_metadata *out) {
    int value_index;
    int assets_index;
    int item_index;
    int item;

    memset(out, 0, sizeof(*out));

    value_index = find_object_value(release_json, tokens, object_index, "draft");
    if (value_index < 0 || !token_is_false(release_json, &tokens[value_index]))
        return 0;

    value_index = find_object_value(release_json, tokens, object_index, "prerelease");
    if (value_index < 0 || !token_is_false(release_json, &tokens[value_index]))
        return 0;

    value_index = find_object_value(release_json, tokens, object_index, "tag_name");
    if (value_index < 0 ||
        token_copy(out->runtime_version, sizeof(out->runtime_version), release_json, &tokens[value_index]) != 0)
        return -1;

    value_index = find_object_value(release_json, tokens, object_index, "published_at");
    if (value_index >= 0) {
        token_copy(out->published_at, sizeof(out->published_at), release_json, &tokens[value_index]);
    }

    assets_index = find_object_value(release_json, tokens, object_index, "assets");
    if (assets_index < 0 || tokens[assets_index].type != JSMN_ARRAY)
        return 0;

    item_index = assets_index + 1;
    for (item = 0; item < tokens[assets_index].size; item++) {
        int rc;

        if (tokens[item_index].type != JSMN_OBJECT)
            return -1;

        rc = copy_release_asset(release_json, tokens, item_index, asset_name, out);
        if (rc < 0)
            return -1;
        if (rc > 0)
            return 1;

        item_index = skip_token(tokens, item_index);
    }

    return 0;
}

int parse_release_candidates(const char *releases_json, const char *asset_name,
    remote_metadata *out, size_t out_capacity, size_t *out_count) {
    jsmntok_t tokens[MAX_JSON_TOKENS];
    int count;
    int item_index;
    int item;
    size_t matched = 0;

    if (releases_json == NULL || asset_name == NULL || out == NULL || out_count == NULL || out_capacity == 0)
        return -1;

    memset(out, 0, sizeof(*out) * out_capacity);
    *out_count = 0;

    count = parse_json(releases_json, tokens, MAX_JSON_TOKENS);
    if (count < 1 || tokens[0].type != JSMN_ARRAY)
        return -1;

    item_index = 1;
    for (item = 0; item < tokens[0].size; item++) {
        int rc;

        if (tokens[item_index].type != JSMN_OBJECT)
            return -1;

        rc = parse_release_object(releases_json, tokens, item_index, asset_name, &out[matched]);
        if (rc < 0)
            return -1;
        if (rc > 0) {
            matched++;
            if (matched >= out_capacity)
                break;
        }

        item_index = skip_token(tokens, item_index);
    }

    *out_count = matched;
    return matched > 0 ? 0 : -1;
}

int parse_install_state(const char *manifest_json, install_state *out) {
    jsmntok_t tokens[MAX_JSON_TOKENS];
    int count;
    int value_index;

    if (manifest_json == NULL || out == NULL)
        return -1;

    memset(out, 0, sizeof(*out));

    count = parse_json(manifest_json, tokens, MAX_JSON_TOKENS);
    if (count < 1 || tokens[0].type != JSMN_OBJECT)
        return -1;

    value_index = find_object_value(manifest_json, tokens, 0, "installed_runtime_source");
    if (value_index >= 0)
        token_copy(out->installed_runtime_source, sizeof(out->installed_runtime_source), manifest_json, &tokens[value_index]);

    value_index = find_object_value(manifest_json, tokens, 0, "installed_runtime_version");
    if (value_index >= 0)
        token_copy(out->installed_runtime_version, sizeof(out->installed_runtime_version), manifest_json, &tokens[value_index]);

    value_index = find_object_value(manifest_json, tokens, 0, "installed_runtime_asset_name");
    if (value_index >= 0)
        token_copy(out->installed_runtime_asset_name, sizeof(out->installed_runtime_asset_name), manifest_json, &tokens[value_index]);

    value_index = find_object_value(manifest_json, tokens, 0, "installed_runtime_asset_url");
    if (value_index >= 0)
        token_copy(out->installed_runtime_asset_url, sizeof(out->installed_runtime_asset_url), manifest_json, &tokens[value_index]);

    value_index = find_object_value(manifest_json, tokens, 0, "installed_at");
    if (value_index >= 0)
        token_copy(out->installed_at, sizeof(out->installed_at), manifest_json, &tokens[value_index]);

    value_index = find_object_value(manifest_json, tokens, 0, "last_checked_at");
    if (value_index >= 0)
        token_copy(out->last_checked_at, sizeof(out->last_checked_at), manifest_json, &tokens[value_index]);

    return out->installed_runtime_version[0] == '\0' ? -1 : 0;
}

int format_install_state_json(const install_state *state, char *buffer, size_t buffer_size) {
    int written;

    if (state == NULL || buffer == NULL || buffer_size == 0)
        return -1;

    written = snprintf(buffer, buffer_size,
        "{\n"
        "  \"installed_runtime_source\": \"%s\",\n"
        "  \"installed_runtime_version\": \"%s\",\n"
        "  \"installed_runtime_asset_name\": \"%s\",\n"
        "  \"installed_runtime_asset_url\": \"%s\",\n"
        "  \"installed_at\": \"%s\",\n"
        "  \"last_checked_at\": \"%s\"\n"
        "}\n",
        state->installed_runtime_source,
        state->installed_runtime_version,
        state->installed_runtime_asset_name,
        state->installed_runtime_asset_url,
        state->installed_at,
        state->last_checked_at);
    if (written < 0 || (size_t)written >= buffer_size)
        return -1;

    return 0;
}
