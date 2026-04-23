#include "ui.h"

#include "apostrophe.h"
#include "apostrophe_widgets.h"

static void format_release_date(const char *published_at, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0)
        return;

    if (published_at != NULL && published_at[0] != '\0' && strlen(published_at) >= 10) {
        snprintf(buffer, buffer_size, "%.10s", published_at);
    } else {
        snprintf(buffer, buffer_size, "unknown");
    }
}

int run_updater_ui(const status_model *model, ui_updater_choice *choice) {
    ap_theme *theme;
    TTF_Font *msg_font;
    ap_footer_item footer[] = {
        { .button = AP_BTN_Y, .label = "SETTINGS" },
        { .button = AP_BTN_X, .label = "REINSTALL" },
        { .button = AP_BTN_B, .label = "QUIT" },
        { .button = AP_BTN_A, .label = model->action == ACTION_NONE ? "CLOSE" : "RUN", .is_confirm = true },
    };
    int screen_w;
    int screen_h;
    int msg_h;
    int base_y;
    int rc = AP_CANCELLED;
    int running = 1;

    if (model == NULL || choice == NULL)
        return AP_ERROR;

    *choice = UI_UPDATER_CANCEL;
    theme = ap_get_theme();
    screen_w = ap_get_screen_width();
    screen_h = ap_get_screen_height();
    msg_font = ap_get_font(AP_FONT_MEDIUM);
    if (theme == NULL || msg_font == NULL)
        return AP_ERROR;

    msg_h = TTF_FontHeight(msg_font);
    base_y = (screen_h - msg_h - ap_get_footer_height()) / 2;

    while (running) {
        ap_input_event ev;

        while (ap_poll_input(&ev)) {
            if (!ev.pressed)
                continue;

            switch (ev.button) {
                case AP_BTN_A:
                    *choice = UI_UPDATER_RUN;
                    rc = AP_OK;
                    running = 0;
                    break;
                case AP_BTN_X:
                    *choice = UI_UPDATER_FORCE_REINSTALL;
                    rc = AP_OK;
                    running = 0;
                    break;
                case AP_BTN_Y:
                    *choice = UI_UPDATER_OPEN_SETTINGS;
                    rc = AP_OK;
                    running = 0;
                    break;
                case AP_BTN_B:
                    *choice = UI_UPDATER_CANCEL;
                    rc = AP_CANCELLED;
                    running = 0;
                    break;
                case AP_BTN_MENU:
                    ap_show_footer_overflow();
                    break;
                default:
                    break;
            }
        }

        ap_draw_background();
        ap_draw_text_wrapped(msg_font, model->body,
            ap_scale(40), base_y,
            screen_w - ap_scale(80),
            theme->text, AP_ALIGN_CENTER);
        ap_draw_footer(footer, 4);
        ap_present();
    }

    return rc;
}

int show_unsupported_warning(const char *message, ui_warning_choice *choice) {
    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "BACK" },
        { .button = AP_BTN_A, .label = "ACCEPT", .is_confirm = true },
    };
    ap_message_opts opts = {
        .message = message,
        .footer = footer,
        .footer_count = 2,
    };
    ap_confirm_result result = {0};
    int rc;

    if (message == NULL || choice == NULL)
        return AP_ERROR;

    *choice = UI_WARNING_CANCEL;
    rc = ap_confirmation(&opts, &result);
    if (rc == AP_OK)
        *choice = UI_WARNING_ACCEPT;
    return rc;
}

int show_version_picker(const remote_metadata *releases, int count, int current_index, int *selected_index) {
    ap_list_item items[MAX_REMOTE_RELEASES];
    char trailing_text[MAX_REMOTE_RELEASES][16];
    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "BACK" },
        { .button = AP_BTN_A, .label = "SELECT", .is_confirm = true },
    };
    ap_list_opts opts;
    ap_list_result result;
    int index;

    if (releases == NULL || selected_index == NULL || count <= 0 || count > MAX_REMOTE_RELEASES)
        return AP_ERROR;

    memset(items, 0, sizeof(items));
    memset(trailing_text, 0, sizeof(trailing_text));
    for (index = 0; index < count; index++) {
        format_release_date(releases[index].published_at, trailing_text[index], sizeof(trailing_text[index]));
        items[index].label = releases[index].runtime_version;
        items[index].trailing_text = trailing_text[index];
    }

    opts = ap_list_default_opts("Choose Version", items, count);
    opts.footer = footer;
    opts.footer_count = 2;
    opts.initial_index = current_index;
    opts.confirm_button = AP_BTN_A;

    if (ap_list(&opts, &result) != AP_OK || result.action == AP_ACTION_BACK || result.selected_index < 0)
        return AP_CANCELLED;

    *selected_index = result.selected_index;
    return AP_OK;
}

int show_settings_screen(const remote_metadata *releases, int count,
    int current_index, controller_layout current_layout,
    int *selected_index, controller_layout *selected_layout) {
    ap_option layout_options[] = {
        { .label = "X360", .value = "x360" },
        { .label = "Nintendo", .value = "nintendo" },
    };
    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "BACK" },
        { .button = AP_BTN_LEFT, .label = "CHANGE", .button_text = "\xe2\x86\x90/\xe2\x86\x92" },
        { .button = AP_BTN_A, .label = "SELECT", .is_confirm = true },
        { .button = AP_BTN_X, .label = "SAVE" },
    };
    int staged_index;
    controller_layout staged_layout;
    int focus_index = 0;
    int visible_start = 0;

    if (releases == NULL || selected_index == NULL || selected_layout == NULL ||
            count <= 0 || count > MAX_REMOTE_RELEASES || current_index < 0 || current_index >= count)
        return AP_ERROR;

    staged_index = current_index;
    staged_layout = current_layout;

    for (;;) {
        char version_label[96];
        ap_option version_option[1];
        ap_options_item items[2];
        ap_options_list_opts opts;
        ap_options_list_result result = {0};
        int rc;

        snprintf(version_label, sizeof(version_label), "%.95s", releases[staged_index].runtime_version);
        version_option[0].label = version_label;
        version_option[0].value = version_label;

        memset(items, 0, sizeof(items));
        items[0].label = "Controller Layout";
        items[0].type = AP_OPT_STANDARD;
        items[0].options = layout_options;
        items[0].option_count = 2;
        items[0].selected_option = staged_layout == CONTROLLER_LAYOUT_NINTENDO ? 1 : 0;

        items[1].label = "PortMaster Version";
        items[1].type = AP_OPT_CLICKABLE;
        items[1].options = version_option;
        items[1].option_count = 1;
        items[1].selected_option = 0;

        memset(&opts, 0, sizeof(opts));
        opts.title = "Settings";
        opts.items = items;
        opts.item_count = 2;
        opts.footer = footer;
        opts.footer_count = 4;
        opts.initial_selected_index = focus_index;
        opts.visible_start_index = visible_start;
        opts.action_button = AP_BTN_X;
        opts.label_font = ap_get_font(AP_FONT_MEDIUM);
        opts.value_font = ap_get_font(AP_FONT_TINY);

        rc = ap_options_list(&opts, &result);
        if (rc != AP_OK || result.action == AP_ACTION_BACK)
            return AP_CANCELLED;

        focus_index = result.focused_index >= 0 ? result.focused_index : focus_index;
        visible_start = result.visible_start_index;
        staged_layout = result.items[0].selected_option == 1
            ? CONTROLLER_LAYOUT_NINTENDO : CONTROLLER_LAYOUT_X360;

        if (result.action == AP_ACTION_TRIGGERED) {
            *selected_index = staged_index;
            *selected_layout = staged_layout;
            return AP_OK;
        }

        if (result.action == AP_ACTION_SELECTED && result.focused_index == 1) {
            int picker_index = staged_index;

            if (show_version_picker(releases, count, staged_index, &picker_index) == AP_OK &&
                    picker_index >= 0 && picker_index < count) {
                staged_index = picker_index;
            }
            focus_index = 1;
        }
    }
}

int show_message_box(const char *message) {
    ap_footer_item footer[] = {
        { .button = AP_BTN_A, .label = "OK", .is_confirm = true },
    };
    ap_message_opts opts = {
        .message = message,
        .footer = footer,
        .footer_count = 1,
    };
    ap_confirm_result result = {0};

    return ap_confirmation(&opts, &result);
}

int show_process_dialog(const char *message, ui_process_fn fn, void *userdata,
    float *progress, int *interrupt_signal, char **dynamic_message) {
    ap_process_opts opts = {
        .message = message,
        .show_progress = true,
        .progress = progress,
        .interrupt_signal = interrupt_signal,
        .interrupt_button = AP_BTN_B,
        .dynamic_message = dynamic_message,
        .message_lines = 3,
    };

    return ap_process_message(&opts, (ap_process_fn)fn, userdata);
}
