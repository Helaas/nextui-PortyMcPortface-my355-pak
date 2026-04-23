#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/consent.h"
#include "../src/userdata.h"

static void join_path(char *buffer, size_t buffer_size, const char *root, const char *suffix) {
    snprintf(buffer, buffer_size, "%s/%s", root, suffix);
}

int main(void) {
    char root_template[] = "/tmp/pm-consent-test-XXXXXX";
    char *root = mkdtemp(root_template);
    char path[PATH_MAX];
    char expected[PATH_MAX];
    char userdata_root[PATH_MAX];
    char shared_root[PATH_MAX];
    install_layout layout = {0};
    warning_consent saved = {0};
    warning_consent loaded = {0};
    const char *warning;

    assert(root != NULL);
    layout.sdcard_root = root;
    layout.platform_name = "my355";

    unsetenv("SHARED_USERDATA_PATH");
    unsetenv("USERDATA_PATH");
    assert(portmaster_userdata_dir_for_layout(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/.userdata/my355/PORTS-portmaster", root);
    assert(strcmp(path, expected) == 0);
    assert(consent_path_for_layout(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/.userdata/my355/PORTS-portmaster/warning-consent.json", root);
    assert(strcmp(path, expected) == 0);

    assert(load_warning_consent(&layout, &loaded) == 0);
    assert(loaded.accepted == 0);
    assert(loaded.accepted_at[0] == '\0');

    join_path(userdata_root, sizeof(userdata_root), root, "userdata-local");
    assert(setenv("USERDATA_PATH", userdata_root, 1) == 0);
    assert(portmaster_userdata_dir_for_layout(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/PORTS-portmaster", userdata_root);
    assert(strcmp(path, expected) == 0);
    assert(consent_path_for_layout(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/PORTS-portmaster/warning-consent.json", userdata_root);
    assert(strcmp(path, expected) == 0);

    join_path(shared_root, sizeof(shared_root), root, "userdata-shared");
    assert(setenv("SHARED_USERDATA_PATH", shared_root, 1) == 0);
    assert(portmaster_userdata_dir_for_layout(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/PORTS-portmaster", shared_root);
    assert(strcmp(path, expected) == 0);
    assert(consent_path_for_layout(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/PORTS-portmaster/warning-consent.json", shared_root);
    assert(strcmp(path, expected) == 0);

    saved.accepted = 1;
    strcpy(saved.accepted_at, "2026-04-16T22:00:00Z");
    assert(save_warning_consent(&layout, &saved) == 0);

    memset(&loaded, 0, sizeof(loaded));
    assert(load_warning_consent(&layout, &loaded) == 0);
    assert(loaded.accepted == 1);
    assert(strcmp(loaded.accepted_at, "2026-04-16T22:00:00Z") == 0);

    warning = unsupported_warning_message();
    assert(strstr(warning, "not an officially supported PortMaster") != NULL);
    assert(strstr(warning, "100% compatibility with all ports") != NULL);
    assert(strstr(warning, "Do not report") != NULL);

    return 0;
}
