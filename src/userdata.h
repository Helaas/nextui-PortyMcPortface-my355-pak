#ifndef USERDATA_H
#define USERDATA_H

#include <stddef.h>

#include "install.h"

int portmaster_userdata_dir_for_layout(const install_layout *layout, char *buffer, size_t buffer_size);

#endif
