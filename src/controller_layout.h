#ifndef CONTROLLER_LAYOUT_H
#define CONTROLLER_LAYOUT_H

#include <stddef.h>

#include "install.h"

typedef enum {
    CONTROLLER_LAYOUT_X360 = 0,
    CONTROLLER_LAYOUT_NINTENDO
} controller_layout;

const char *controller_layout_label(controller_layout layout);
int controller_layout_sentinel_path(const install_layout *layout, char *buffer, size_t buffer_size);
int load_controller_layout(const install_layout *layout, controller_layout *out);
int save_controller_layout(const install_layout *layout, controller_layout value);

#endif
