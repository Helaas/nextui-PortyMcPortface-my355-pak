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
    char ports_dir[PATH_MAX];
    char port_script[PATH_MAX];
    struct stat st = {0};
    install_layout layout = {0};
    controller_layout loaded = CONTROLLER_LAYOUT_NINTENDO;
    port_controller_layout loaded_port = PORT_CONTROLLER_LAYOUT_NINTENDO;
    port_controller_layout_entry *entries = NULL;
    int entry_count = 0;

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

    assert(controller_layout_overrides_dir(&layout, path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/PORTS-portmaster/controller-layouts", shared_root);
    assert(strcmp(path, expected) == 0);
    assert(port_controller_layout_override_path(&layout, "TestPort.sh", path, sizeof(path)) == 0);
    snprintf(expected, sizeof(expected), "%s/PORTS-portmaster/controller-layouts/TestPort.sh.layout", shared_root);
    assert(strcmp(path, expected) == 0);
    assert(load_port_controller_layout(&layout, "TestPort.sh", &loaded_port) == 0);
    assert(loaded_port == PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL);

    assert(save_port_controller_layout(&layout, "TestPort.sh", PORT_CONTROLLER_LAYOUT_NINTENDO) == 0);
    assert(stat(expected, &st) == 0);
    assert(load_port_controller_layout(&layout, "TestPort.sh", &loaded_port) == 0);
    assert(loaded_port == PORT_CONTROLLER_LAYOUT_NINTENDO);

    assert(save_port_controller_layout(&layout, "TestPort.sh", PORT_CONTROLLER_LAYOUT_X360) == 0);
    assert(load_port_controller_layout(&layout, "TestPort.sh", &loaded_port) == 0);
    assert(loaded_port == PORT_CONTROLLER_LAYOUT_X360);

    assert(save_port_controller_layout(&layout, "TestPort.sh", PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL) == 0);
    assert(stat(expected, &st) != 0);
    assert(load_port_controller_layout(&layout, "TestPort.sh", &loaded_port) == 0);
    assert(loaded_port == PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL);

    assert(port_controller_layout_override_path(&layout, "", path, sizeof(path)) != 0);
    assert(port_controller_layout_override_path(&layout, "bad/name.sh", path, sizeof(path)) != 0);
    assert(save_port_controller_layout(&layout, "bad/name.sh", PORT_CONTROLLER_LAYOUT_X360) != 0);

    snprintf(ports_dir, sizeof(ports_dir), "%s/Roms/Ports (PORTS)/.ports", root);
    assert(fs_ensure_dir(ports_dir) == 0);
    snprintf(port_script, sizeof(port_script), "%s/Zeta Port.sh", ports_dir);
    assert(write_text_file(port_script, "#!/bin/sh\n") == 0);
    snprintf(port_script, sizeof(port_script), "%s/Alpha.sh", ports_dir);
    assert(write_text_file(port_script, "#!/bin/sh\n") == 0);
    snprintf(port_script, sizeof(port_script), "%s/README.txt", ports_dir);
    assert(write_text_file(port_script, "ignored\n") == 0);
    assert(save_port_controller_layout(&layout, "Zeta Port.sh", PORT_CONTROLLER_LAYOUT_X360) == 0);
    assert(list_port_controller_layouts(&layout, &entries, &entry_count) == 0);
    assert(entry_count == 2);
    assert(strcmp(entries[0].launcher_name, "Alpha.sh") == 0);
    assert(strcmp(entries[0].display_name, "Alpha") == 0);
    assert(entries[0].layout == PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL);
    assert(strcmp(entries[1].launcher_name, "Zeta Port.sh") == 0);
    assert(strcmp(entries[1].display_name, "Zeta Port") == 0);
    assert(entries[1].layout == PORT_CONTROLLER_LAYOUT_X360);
    free_port_controller_layouts(entries);

    assert(strcmp(controller_layout_label(CONTROLLER_LAYOUT_X360), "X360") == 0);
    assert(strcmp(controller_layout_label(CONTROLLER_LAYOUT_NINTENDO), "Nintendo") == 0);
    assert(strcmp(port_controller_layout_label(PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL), "Follow Global") == 0);
    assert(strcmp(port_controller_layout_label(PORT_CONTROLLER_LAYOUT_X360), "X360") == 0);
    assert(strcmp(port_controller_layout_label(PORT_CONTROLLER_LAYOUT_NINTENDO), "Nintendo") == 0);

    return 0;
}
