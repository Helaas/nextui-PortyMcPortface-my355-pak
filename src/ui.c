#include "ui.h"

#include "apostrophe.h"
#include "apostrophe_widgets.h"

#include <stdlib.h>
#include <string.h>

static int show_port_layouts_screen(const install_layout *layout);

int run_updater_ui(const status_model *model, ui_updater_choice *choice) {
    ap_theme *theme;
    TTF_Font *msg_font;
    ap_footer_item footer[4];
    int footer_count = 0;
    int screen_w;
    int screen_h;
    int msg_h;
    int base_y;
    int rc = AP_CANCELLED;
    int running = 1;

    if (model == NULL || choice == NULL)
        return AP_ERROR;

    footer[footer_count++] = (ap_footer_item){ .button = AP_BTN_B, .label = "QUIT" };
    footer[footer_count++] = (ap_footer_item){ .button = AP_BTN_X, .label = "REINSTALL" };
    footer[footer_count++] = (ap_footer_item){ .button = AP_BTN_Y, .label = "SETTINGS", .is_confirm = true };
    if (model->action != ACTION_NONE) {
        footer[footer_count++] = (ap_footer_item){ .button = AP_BTN_A, .label = "RUN", .is_confirm = true };
    }

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
                    if (model->action != ACTION_NONE) {
                        *choice = UI_UPDATER_RUN;
                        rc = AP_OK;
                        running = 0;
                    }
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
        ap_draw_footer(footer, footer_count);
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

int show_settings_screen(const install_layout *layout, const remote_metadata *releases, int count,
    int current_index, controller_layout current_layout,
    int *selected_index, controller_layout *selected_layout) {
    ap_option layout_options[] = {
        { .label = "X360", .value = "x360" },
        { .label = "Nintendo", .value = "nintendo" },
    };
    ap_option configure_options[] = {
        { .label = "Configure", .value = "configure" },
    };
    ap_footer_item footer[] = {
        { .button = AP_BTN_LEFT, .label = "CHANGE", .button_text = "\xe2\x86\x90/\xe2\x86\x92" },
        { .button = AP_BTN_A, .label = "OPEN", .is_confirm = true },
        { .button = AP_BTN_B, .label = "BACK" },
        { .button = AP_BTN_START, .label = "SAVE", .is_confirm = true },
    };
    ap_option version_options[MAX_REMOTE_RELEASES];
    ap_options_item items[3];
    ap_options_list_opts opts;
    ap_options_list_result result = {0};
    int index;
    int focused_index = 0;
    int visible_start_index = 0;

    if (layout == NULL || releases == NULL || selected_index == NULL || selected_layout == NULL ||
            count <= 0 || count > MAX_REMOTE_RELEASES || current_index < 0 || current_index >= count)
        return AP_ERROR;

    memset(version_options, 0, sizeof(version_options));
    for (index = 0; index < count; index++) {
        version_options[index].label = releases[index].runtime_version;
        version_options[index].value = releases[index].runtime_version;
    }

    memset(items, 0, sizeof(items));
    items[0].label = "Global Controller Layout";
    items[0].type = AP_OPT_STANDARD;
    items[0].options = layout_options;
    items[0].option_count = 2;
    items[0].selected_option = current_layout == CONTROLLER_LAYOUT_NINTENDO ? 1 : 0;

    items[1].label = "Port Layouts";
    items[1].type = AP_OPT_CLICKABLE;
    items[1].options = configure_options;
    items[1].option_count = 1;
    items[1].selected_option = 0;

    items[2].label = "PortMaster Version";
    items[2].type = AP_OPT_STANDARD;
    items[2].options = version_options;
    items[2].option_count = count;
    items[2].selected_option = current_index;

    for (;;) {
        memset(&opts, 0, sizeof(opts));
        memset(&result, 0, sizeof(result));
        opts.title = "Settings";
        opts.items = items;
        opts.item_count = 3;
        opts.footer = footer;
        opts.footer_count = 4;
        opts.initial_selected_index = focused_index;
        opts.visible_start_index = visible_start_index;
        opts.confirm_button = AP_BTN_START;
        opts.label_font = ap_get_font(AP_FONT_MEDIUM);
        opts.value_font = ap_get_font(AP_FONT_TINY);

        if (ap_options_list(&opts, &result) != AP_OK || result.action == AP_ACTION_BACK)
            return AP_CANCELLED;

        focused_index = result.focused_index;
        visible_start_index = result.visible_start_index;
        if (result.action == AP_ACTION_SELECTED && result.focused_index == 1) {
            show_port_layouts_screen(layout);
            continue;
        }

        if (result.action != AP_ACTION_CONFIRMED)
            continue;

        break;
    }

    if (items[0].selected_option < 0 || items[0].selected_option >= 2 ||
            items[2].selected_option < 0 || items[2].selected_option >= count)
        return AP_ERROR;

    *selected_layout = items[0].selected_option == 1
        ? CONTROLLER_LAYOUT_NINTENDO : CONTROLLER_LAYOUT_X360;
    *selected_index = items[2].selected_option;
    return AP_OK;
}

static int show_port_layouts_screen(const install_layout *layout) {
    ap_option layout_options[] = {
        { .label = "Follow Global", .value = "global" },
        { .label = "X360", .value = "x360" },
        { .label = "Nintendo", .value = "nintendo" },
    };
    ap_footer_item footer[] = {
        { .button = AP_BTN_LEFT, .label = "CHANGE", .button_text = "\xe2\x86\x90/\xe2\x86\x92" },
        { .button = AP_BTN_B, .label = "BACK" },
        { .button = AP_BTN_START, .label = "SAVE", .is_confirm = true },
    };
    port_controller_layout_entry *entries = NULL;
    ap_options_item *items = NULL;
    ap_options_list_opts opts;
    ap_options_list_result result = {0};
    int count = 0;
    int index;
    int rc = AP_CANCELLED;

    if (layout == NULL)
        return AP_ERROR;
    if (list_port_controller_layouts(layout, &entries, &count) != 0) {
        show_message_box("Could not load installed ports.");
        return AP_ERROR;
    }
    if (count == 0) {
        free_port_controller_layouts(entries);
        show_message_box("No installed ports found.");
        return AP_CANCELLED;
    }

    items = calloc((size_t)count, sizeof(*items));
    if (items == NULL) {
        free_port_controller_layouts(entries);
        return AP_ERROR;
    }

    for (index = 0; index < count; index++) {
        items[index].label = entries[index].display_name;
        items[index].type = AP_OPT_STANDARD;
        items[index].options = layout_options;
        items[index].option_count = 3;
        items[index].selected_option = entries[index].layout;
    }

    memset(&opts, 0, sizeof(opts));
    opts.title = "Port Layouts";
    opts.items = items;
    opts.item_count = count;
    opts.footer = footer;
    opts.footer_count = 3;
    opts.confirm_button = AP_BTN_START;
    opts.label_font = ap_get_font(AP_FONT_MEDIUM);
    opts.value_font = ap_get_font(AP_FONT_TINY);

    if (ap_options_list(&opts, &result) == AP_OK && result.action == AP_ACTION_CONFIRMED) {
        rc = AP_OK;
        for (index = 0; index < count; index++) {
            if (items[index].selected_option < PORT_CONTROLLER_LAYOUT_FOLLOW_GLOBAL ||
                    items[index].selected_option > PORT_CONTROLLER_LAYOUT_NINTENDO ||
                    save_port_controller_layout(layout, entries[index].launcher_name,
                        (port_controller_layout)items[index].selected_option) != 0) {
                show_message_box("Could not save port layouts.");
                rc = AP_ERROR;
                break;
            }
        }
    }

    free(items);
    free_port_controller_layouts(entries);
    return rc;
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
