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
        { .button = AP_BTN_Y, .label = "VERSION" },
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
                    *choice = UI_UPDATER_PICK_VERSION;
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
