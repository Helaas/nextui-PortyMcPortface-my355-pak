#include "controller_layout.h"

#include "fs.h"
#include "userdata.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONTROLLER_LAYOUT_SENTINEL "nintendo"

static int name_is_nintendo_sentinel(const char *name) {
    return name != NULL && strncasecmp(name, CONTROLLER_LAYOUT_SENTINEL,
        strlen(CONTROLLER_LAYOUT_SENTINEL)) == 0;
}

static int find_nintendo_sentinel(const char *dir_path) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(dir_path);
    if (dir == NULL)
        return errno == ENOENT ? 0 : -1;

    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        struct stat st;

        if (!name_is_nintendo_sentinel(entry->d_name))
            continue;
        if (snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name) >= (int)sizeof(path)) {
            closedir(dir);
            return -1;
        }
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            closedir(dir);
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

static int remove_nintendo_sentinels(const char *dir_path) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(dir_path);
    if (dir == NULL)
        return errno == ENOENT ? 0 : -1;

    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        struct stat st;

        if (!name_is_nintendo_sentinel(entry->d_name))
            continue;
        if (snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name) >= (int)sizeof(path)) {
            closedir(dir);
            return -1;
        }
        if (stat(path, &st) != 0)
            continue;
        if (!S_ISREG(st.st_mode))
            continue;
        if (unlink(path) != 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    return 0;
}

const char *controller_layout_label(controller_layout layout) {
    return layout == CONTROLLER_LAYOUT_NINTENDO ? "Nintendo" : "X360";
}

int controller_layout_sentinel_path(const install_layout *layout, char *buffer, size_t buffer_size) {
    char dir[PATH_MAX];

    if (portmaster_userdata_dir_for_layout(layout, dir, sizeof(dir)) != 0)
        return -1;
    return snprintf(buffer, buffer_size, "%s/%s", dir, CONTROLLER_LAYOUT_SENTINEL) < (int)buffer_size ? 0 : -1;
}

int load_controller_layout(const install_layout *layout, controller_layout *out) {
    char dir[PATH_MAX];
    int found;

    if (out == NULL)
        return -1;
    *out = CONTROLLER_LAYOUT_X360;
    if (portmaster_userdata_dir_for_layout(layout, dir, sizeof(dir)) != 0)
        return -1;

    found = find_nintendo_sentinel(dir);
    if (found < 0)
        return -1;
    *out = found ? CONTROLLER_LAYOUT_NINTENDO : CONTROLLER_LAYOUT_X360;
    return 0;
}

int save_controller_layout(const install_layout *layout, controller_layout value) {
    char dir[PATH_MAX];
    char sentinel_path[PATH_MAX];

    if (portmaster_userdata_dir_for_layout(layout, dir, sizeof(dir)) != 0)
        return -1;

    if (value == CONTROLLER_LAYOUT_NINTENDO) {
        if (controller_layout_sentinel_path(layout, sentinel_path, sizeof(sentinel_path)) != 0)
            return -1;
        return write_text_file(sentinel_path, "");
    }

    return remove_nintendo_sentinels(dir);
}
