#include "controller_layout.h"

#include "fs.h"
#include "userdata.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONTROLLER_LAYOUT_SENTINEL "nintendo"
#define CONTROLLER_LAYOUT_OVERRIDES_DIR "controller-layouts"

static int launcher_name_is_safe(const char *launcher_name) {
    return launcher_name != NULL && launcher_name[0] != '\0' &&
        strchr(launcher_name, '/') == NULL &&
        strcmp(launcher_name, ".") != 0 &&
        strcmp(launcher_name, "..") != 0;
}

static int name_has_suffix(const char *name, const char *suffix) {
    size_t name_len;
    size_t suffix_len;

    if (name == NULL || suffix == NULL)
        return 0;
    name_len = strlen(name);
    suffix_len = strlen(suffix);
    return name_len >= suffix_len &&
        strcmp(name + name_len - suffix_len, suffix) == 0;
}

static int compare_port_entries(const void *left, const void *right) {
    const port_controller_layout_entry *a = left;
    const port_controller_layout_entry *b = right;
    int cmp = strcasecmp(a->display_name, b->display_name);

    if (cmp != 0)
        return cmp;
    return strcasecmp(a->launcher_name, b->launcher_name);
}

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

const char *port_controller_layout_label(port_controller_layout layout) {
    switch (layout) {
        case PORT_CONTROLLER_LAYOUT_X360:
            return "X360";
        case PORT_CONTROLLER_LAYOUT_NINTENDO:
            return "Nintendo";
        case PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL:
        default:
            return "Follow Global";
    }
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

int controller_layout_overrides_dir(const install_layout *layout, char *buffer, size_t buffer_size) {
    char dir[PATH_MAX];

    if (portmaster_userdata_dir_for_layout(layout, dir, sizeof(dir)) != 0)
        return -1;
    return snprintf(buffer, buffer_size, "%s/%s", dir, CONTROLLER_LAYOUT_OVERRIDES_DIR) < (int)buffer_size ? 0 : -1;
}

int port_controller_layout_override_path(const install_layout *layout, const char *launcher_name,
    char *buffer, size_t buffer_size) {
    char dir[PATH_MAX];

    if (!launcher_name_is_safe(launcher_name))
        return -1;
    if (controller_layout_overrides_dir(layout, dir, sizeof(dir)) != 0)
        return -1;
    return snprintf(buffer, buffer_size, "%s/%s.layout", dir, launcher_name) < (int)buffer_size ? 0 : -1;
}

int load_port_controller_layout(const install_layout *layout, const char *launcher_name,
    port_controller_layout *out) {
    char path[PATH_MAX];
    char *content = NULL;
    char value[32] = {0};
    size_t len = 0;

    if (out == NULL)
        return -1;
    *out = PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL;
    if (port_controller_layout_override_path(layout, launcher_name, path, sizeof(path)) != 0)
        return -1;

    if (read_text_file_alloc(path, &content, &len) != 0) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    if (sscanf(content, " %31s", value) == 1) {
        if (strcasecmp(value, "nintendo") == 0) {
            *out = PORT_CONTROLLER_LAYOUT_NINTENDO;
        } else if (strcasecmp(value, "x360") == 0 || strcasecmp(value, "xbox") == 0) {
            *out = PORT_CONTROLLER_LAYOUT_X360;
        }
    }
    free(content);
    return 0;
}

int save_port_controller_layout(const install_layout *layout, const char *launcher_name,
    port_controller_layout value) {
    char path[PATH_MAX];

    if (port_controller_layout_override_path(layout, launcher_name, path, sizeof(path)) != 0)
        return -1;

    switch (value) {
        case PORT_CONTROLLER_LAYOUT_NINTENDO:
            return write_text_file(path, "nintendo\n");
        case PORT_CONTROLLER_LAYOUT_X360:
            return write_text_file(path, "x360\n");
        case PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL:
            if (unlink(path) != 0 && errno != ENOENT)
                return -1;
            return 0;
        default:
            return -1;
    }
}

int list_port_controller_layouts(const install_layout *layout,
    port_controller_layout_entry **out_entries, int *out_count) {
    char ports_dir[PATH_MAX];
    DIR *dir;
    struct dirent *entry;
    port_controller_layout_entry *entries = NULL;
    int count = 0;
    int capacity = 0;

    if (layout == NULL || layout->sdcard_root == NULL || out_entries == NULL || out_count == NULL)
        return -1;
    *out_entries = NULL;
    *out_count = 0;

    if (snprintf(ports_dir, sizeof(ports_dir), "%s/Roms/Ports (PORTS)/.ports", layout->sdcard_root) >= (int)sizeof(ports_dir))
        return -1;

    dir = opendir(ports_dir);
    if (dir == NULL)
        return errno == ENOENT ? 0 : -1;

    while ((entry = readdir(dir)) != NULL) {
        char launcher_path[PATH_MAX];
        struct stat st;
        port_controller_layout_entry *next_entries;
        size_t name_len;

        if (!launcher_name_is_safe(entry->d_name) || !name_has_suffix(entry->d_name, ".sh"))
            continue;
        if (snprintf(launcher_path, sizeof(launcher_path), "%s/%s", ports_dir, entry->d_name) >= (int)sizeof(launcher_path)) {
            closedir(dir);
            free(entries);
            return -1;
        }
        if (stat(launcher_path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        if (count == capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            next_entries = realloc(entries, (size_t)capacity * sizeof(*entries));
            if (next_entries == NULL) {
                closedir(dir);
                free(entries);
                return -1;
            }
            entries = next_entries;
        }

        memset(&entries[count], 0, sizeof(entries[count]));
        if (snprintf(entries[count].launcher_name, sizeof(entries[count].launcher_name),
                "%s", entry->d_name) >= (int)sizeof(entries[count].launcher_name)) {
            closedir(dir);
            free(entries);
            return -1;
        }

        name_len = strlen(entry->d_name);
        if (name_len > 3)
            name_len -= 3;
        if (name_len >= sizeof(entries[count].display_name)) {
            closedir(dir);
            free(entries);
            return -1;
        }
        memcpy(entries[count].display_name, entry->d_name, name_len);
        entries[count].display_name[name_len] = '\0';
        if (load_port_controller_layout(layout, entries[count].launcher_name, &entries[count].layout) != 0)
            entries[count].layout = PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL;
        count++;
    }

    closedir(dir);
    if (count > 1)
        qsort(entries, (size_t)count, sizeof(*entries), compare_port_entries);

    *out_entries = entries;
    *out_count = count;
    return 0;
}

void free_port_controller_layouts(port_controller_layout_entry *entries) {
    free(entries);
}
