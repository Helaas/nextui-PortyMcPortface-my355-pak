#ifndef UI_H
#define UI_H

#include "controller_layout.h"
#include "status.h"

typedef int (*ui_process_fn)(void *userdata);

typedef enum {
    UI_UPDATER_CANCEL = 0,
    UI_UPDATER_RUN,
    UI_UPDATER_FORCE_REINSTALL,
    UI_UPDATER_OPEN_SETTINGS
} ui_updater_choice;

typedef enum {
    UI_WARNING_CANCEL = 0,
    UI_WARNING_ACCEPT
} ui_warning_choice;

int run_updater_ui(const status_model *model, ui_updater_choice *choice);
int show_unsupported_warning(const char *message, ui_warning_choice *choice);
int show_settings_screen(const install_layout *layout, const remote_metadata *releases, int count,
    int current_index, controller_layout current_layout,
    int *selected_index, controller_layout *selected_layout);
int show_message_box(const char *message);
int show_process_dialog(const char *message, ui_process_fn fn, void *userdata,
    float *progress, int *interrupt_signal, char **dynamic_message);

#endif
