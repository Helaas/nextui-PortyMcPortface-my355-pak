#ifndef CONSENT_H
#define CONSENT_H

#include <stddef.h>

#include "install.h"

typedef struct {
    int accepted;
    char accepted_at[32];
} warning_consent;

int consent_path_for_layout(const install_layout *layout, char *buffer, size_t buffer_size);
int load_warning_consent(const install_layout *layout, warning_consent *out);
int save_warning_consent(const install_layout *layout, const warning_consent *consent);
const char *unsupported_warning_message(void);

#endif
