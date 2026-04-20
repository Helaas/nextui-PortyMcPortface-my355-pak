#include "consent.h"

#include "fs.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONSENT_FILENAME "warning-consent.json"

static int consent_dir_for_layout(const install_layout *layout, char *buffer, size_t buffer_size) {
    const char *shared = getenv("SHARED_USERDATA_PATH");
    const char *userdata = getenv("USERDATA_PATH");

    if (layout == NULL || buffer == NULL || buffer_size == 0 ||
            layout->sdcard_root == NULL || layout->platform_name == NULL)
        return -1;

    if (shared != NULL && shared[0] != '\0')
        return snprintf(buffer, buffer_size, "%s/PORTS-portmaster", shared) < (int)buffer_size ? 0 : -1;
    if (userdata != NULL && userdata[0] != '\0')
        return snprintf(buffer, buffer_size, "%s/PORTS-portmaster", userdata) < (int)buffer_size ? 0 : -1;
    return snprintf(buffer, buffer_size, "%s/.userdata/%s/PORTS-portmaster",
        layout->sdcard_root, layout->platform_name) < (int)buffer_size ? 0 : -1;
}

int consent_path_for_layout(const install_layout *layout, char *buffer, size_t buffer_size) {
    char dir[PATH_MAX];

    if (consent_dir_for_layout(layout, dir, sizeof(dir)) != 0)
        return -1;

    return snprintf(buffer, buffer_size, "%s/%s", dir, CONSENT_FILENAME) < (int)buffer_size ? 0 : -1;
}

static void clear_warning_consent(warning_consent *out) {
    memset(out, 0, sizeof(*out));
}

static int copy_json_string(const char *json, const char *key, char *dst, size_t dst_size) {
    char needle[64];
    const char *start;
    const char *end;
    size_t len;

    snprintf(needle, sizeof(needle), "\"%s\": \"", key);
    start = strstr(json, needle);
    if (start == NULL)
        return -1;

    start += strlen(needle);
    end = strchr(start, '"');
    if (end == NULL)
        return -1;

    len = (size_t)(end - start);
    if (len + 1 > dst_size)
        return -1;

    memcpy(dst, start, len);
    dst[len] = '\0';
    return 0;
}

int load_warning_consent(const install_layout *layout, warning_consent *out) {
    char path[PATH_MAX];
    char *json = NULL;
    size_t json_len = 0;

    if (out == NULL)
        return -1;

    clear_warning_consent(out);

    if (consent_path_for_layout(layout, path, sizeof(path)) != 0)
        return -1;
    if (!fs_path_exists(path))
        return 0;
    if (read_text_file_alloc(path, &json, &json_len) != 0)
        return -1;

    out->accepted = strstr(json, "\"warning_accepted\": true") != NULL;
    if (copy_json_string(json, "accepted_at", out->accepted_at, sizeof(out->accepted_at)) != 0)
        out->accepted_at[0] = '\0';

    free(json);
    (void)json_len;
    return 0;
}

int save_warning_consent(const install_layout *layout, const warning_consent *consent) {
    char dir[PATH_MAX];
    char path[PATH_MAX];
    char json[256];

    if (consent == NULL)
        return -1;
    if (consent_dir_for_layout(layout, dir, sizeof(dir)) != 0)
        return -1;
    if (consent_path_for_layout(layout, path, sizeof(path)) != 0)
        return -1;
    if (fs_ensure_dir(dir) != 0)
        return -1;

    if (snprintf(json, sizeof(json),
            "{\n"
            "  \"warning_accepted\": %s,\n"
            "  \"accepted_at\": \"%s\"\n"
            "}\n",
            consent->accepted ? "true" : "false",
            consent->accepted_at) >= (int)sizeof(json))
        return -1;

    return write_text_file(path, json);
}

const char *unsupported_warning_message(void) {
    return "This is not an officially supported PortMaster CFW integration.\n\n"
           "Porty McPortface will never have 100% compatibility with all ports.\n\n"
           "Do not report compatibility or integration issues to the official PortMaster project or its maintainers.";
}
