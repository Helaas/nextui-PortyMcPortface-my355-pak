#ifndef CONTROLLER_LAYOUT_H
#define CONTROLLER_LAYOUT_H

#include <limits.h>
#include <stddef.h>

#include "install.h"

typedef enum {
    CONTROLLER_LAYOUT_X360 = 0,
    CONTROLLER_LAYOUT_NINTENDO
} controller_layout;

typedef enum {
    PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL = 0,
    PORT_CONTROLLER_LAYOUT_X360,
    PORT_CONTROLLER_LAYOUT_NINTENDO
} port_controller_layout;

typedef struct {
    char launcher_name[PATH_MAX];
    char display_name[PATH_MAX];
    port_controller_layout layout;
} port_controller_layout_entry;

const char *controller_layout_label(controller_layout layout);
const char *port_controller_layout_label(port_controller_layout layout);
int controller_layout_sentinel_path(const install_layout *layout, char *buffer, size_t buffer_size);
int load_controller_layout(const install_layout *layout, controller_layout *out);
int save_controller_layout(const install_layout *layout, controller_layout value);
int controller_layout_overrides_dir(const install_layout *layout, char *buffer, size_t buffer_size);
int port_controller_layout_override_path(const install_layout *layout, const char *launcher_name,
    char *buffer, size_t buffer_size);
int load_port_controller_layout(const install_layout *layout, const char *launcher_name,
    port_controller_layout *out);
int save_port_controller_layout(const install_layout *layout, const char *launcher_name,
    port_controller_layout value);
int list_port_controller_layouts(const install_layout *layout,
    port_controller_layout_entry **out_entries, int *out_count);
void free_port_controller_layouts(port_controller_layout_entry *entries);

#endif
