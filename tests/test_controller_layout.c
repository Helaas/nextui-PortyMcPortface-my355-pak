#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/controller_layout.h"
#include "../src/fs.h"

static void join_path(char *buffer, size_t buffer_size, const char *root, const char *suffix) {
    snprintf(buffer, buffer_size, "%s/%s", root, suffix);
}

int main(void) {
    char root_template[] = "/tmp/pm-controller-layout-test-XXXXXX";
    char *root = mkdtemp(root_template);
    char path[PATH_MAX];
    char expected[PATH_MAX];
    char userdata_root[PATH_MAX];
    char shared_root[PATH_MAX];
    char stale_path[PATH_MAX];
    struct stat st = {0};
    install_layout layout = {0};
    controller_layout loaded = CONTROLLER_LAYOUT_NINTENDO;

    assert(root != NULL);
    layout.sdcard_root = root;
    layout.platform_name = "my355";

    unsetenv("SHARED_USERDATA_PATH");
    unsetenv("USERDATA_PATH");

    assert(controller_layout_sentinel_path(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/.userdata/my355/PORTS-portmaster/nintendo", root);
    assert(strcmp(path, expected) == 0);
    assert(load_controller_layout(&layout, &loaded) == 0);
    assert(loaded == CONTROLLER_LAYOUT_X360);

    assert(save_controller_layout(&layout, CONTROLLER_LAYOUT_NINTENDO) == 0);
    assert(stat(expected, &st) == 0);
    assert(load_controller_layout(&layout, &loaded) == 0);
    assert(loaded == CONTROLLER_LAYOUT_NINTENDO);

    snprintf(stale_path, sizeof(stale_path), "%s/.userdata/my355/PORTS-portmaster/Nintendo-old", root);
    assert(write_text_file(stale_path, "legacy\n") == 0);
    assert(save_controller_layout(&layout, CONTROLLER_LAYOUT_X360) == 0);
    assert(stat(expected, &st) != 0);
    assert(stat(stale_path, &st) != 0);
    assert(load_controller_layout(&layout, &loaded) == 0);
    assert(loaded == CONTROLLER_LAYOUT_X360);

    join_path(userdata_root, sizeof(userdata_root), root, "userdata-local");
    assert(setenv("USERDATA_PATH", userdata_root, 1) == 0);
    assert(controller_layout_sentinel_path(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/PORTS-portmaster/nintendo", userdata_root);
    assert(strcmp(path, expected) == 0);

    join_path(shared_root, sizeof(shared_root), root, "userdata-shared");
    assert(setenv("SHARED_USERDATA_PATH", shared_root, 1) == 0);
    assert(controller_layout_sentinel_path(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/PORTS-portmaster/nintendo", shared_root);
    assert(strcmp(path, expected) == 0);

    assert(save_controller_layout(&layout, CONTROLLER_LAYOUT_NINTENDO) == 0);
    assert(stat(expected, &st) == 0);
    assert(load_controller_layout(&layout, &loaded) == 0);
    assert(loaded == CONTROLLER_LAYOUT_NINTENDO);

    assert(strcmp(controller_layout_label(CONTROLLER_LAYOUT_X360), "X360") == 0);
    assert(strcmp(controller_layout_label(CONTROLLER_LAYOUT_NINTENDO), "Nintendo") == 0);

    return 0;
}
