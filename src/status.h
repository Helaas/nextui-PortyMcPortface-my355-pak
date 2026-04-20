#ifndef STATUS_H
#define STATUS_H

#include "json.h"

typedef enum {
    ACTION_NONE = 0,
    ACTION_INSTALL,
    ACTION_UPDATE
} action_type;

typedef struct {
    action_type action;
    char title[64];
    char body[256];
} status_model;

status_model resolve_status(install_state installed, remote_metadata remote, int files_present);

#endif
