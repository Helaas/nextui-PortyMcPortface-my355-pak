#include "userdata.h"

#include <stdio.h>
#include <stdlib.h>

int portmaster_userdata_dir_for_layout(const install_layout *layout, char *buffer, size_t buffer_size) {
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
