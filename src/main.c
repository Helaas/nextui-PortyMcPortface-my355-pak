#define AP_IMPLEMENTATION
#include "apostrophe.h"
#define AP_WIDGETS_IMPLEMENTATION
#include "apostrophe_widgets.h"

#include "consent.h"
#include "controller_layout.h"
#include "http.h"
#include "install.h"
#include "json.h"
#include "platform.h"
#include "status.h"
#include "ui.h"
#include "updater_flow.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PORTMASTER_RELEASES_URL "https://api.github.com/repos/PortsMaster/PortMaster-GUI/releases?per_page=30"
#define PORTMASTER_RUNTIME_ASSET "trimui.portmaster.zip"
#define PORTMASTER_RUNTIME_SOURCE "portmaster_gui_trimui"

typedef struct {
    install_layout layout;
    remote_metadata remote;
    install_state target_state;
    char tool_pak_dir[PATH_MAX];
    char status_line[160];
    char error[256];
    char *dynamic_message;
    float progress;
    int interrupt_signal;
} install_job;

static int derive_sdcard_root(const char *tool_pak_dir, char *sdcard_root, size_t sdcard_root_size);
static int build_install_layout(const char *tool_pak_dir, platform_id platform, install_layout *layout,
    char *sdcard_root, size_t sdcard_root_size,
    char *runtime_tools_dir, size_t runtime_tools_dir_size,
    char *payload_template_dir, size_t payload_template_dir_size,
    char *payload_pak_dir, size_t payload_pak_dir_size,
    char *rom_stub_path, size_t rom_stub_path_size,
    char *installed_tool_pak_dir, size_t installed_tool_pak_dir_size,
    char *installed_tool_launch_path, size_t installed_tool_launch_path_size,
    char *installed_tool_metadata_path, size_t installed_tool_metadata_path_size,
    char *manifest_path, size_t manifest_path_size);
static void fill_iso8601_now(char *buffer, size_t buffer_size);
static int load_release_candidates(remote_metadata *releases, size_t capacity, size_t *count);
static void format_status_body(status_model *model, const install_state *installed,
    const remote_metadata *latest_remote, const remote_metadata *selected_remote, int selection_active);
static int run_install_job(void *userdata);
static int format_checked(char *buffer, size_t buffer_size, const char *format, ...);

int main(void) {
    char tool_pak_dir[PATH_MAX];
    char sdcard_root[PATH_MAX];
    char runtime_tools_dir[PATH_MAX];
    char payload_template_dir[PATH_MAX];
    char payload_pak_dir[PATH_MAX];
    char rom_stub_path[PATH_MAX];
    char installed_tool_pak_dir[PATH_MAX];
    char installed_tool_launch_path[PATH_MAX];
    char installed_tool_metadata_path[PATH_MAX];
    char manifest_path[PATH_MAX];
    platform_id platform = platform_from_env(getenv("PLATFORM"), getenv("DEVICE"));
    install_layout layout;
    remote_metadata releases[MAX_REMOTE_RELEASES];
    remote_metadata latest_remote = {0};
    remote_metadata selected_remote = {0};
    install_state installed = {0};
    warning_consent consent = {0};
    controller_layout current_controller_layout = CONTROLLER_LAYOUT_X360;
    status_model model;
    install_job job;
    ui_updater_choice updater_choice = UI_UPDATER_CANCEL;
    size_t release_count = 0;
    int selected_index = 0;
    ap_config cfg = {
        .window_title = "Porty McPortface",
        .log_path = ap_resolve_log_path("porty-mcportface"),
        .is_nextui = AP_PLATFORM_IS_DEVICE,
    };
    int files_present;

    if (platform == PLATFORM_ID_UNKNOWN) {
        fprintf(stderr, "Unsupported platform\n");
        return 1;
    }

    if (getcwd(tool_pak_dir, sizeof(tool_pak_dir)) == NULL) {
        perror("getcwd");
        return 1;
    }

    if (build_install_layout(tool_pak_dir, platform, &layout,
            sdcard_root, sizeof(sdcard_root),
            runtime_tools_dir, sizeof(runtime_tools_dir),
            payload_template_dir, sizeof(payload_template_dir),
            payload_pak_dir, sizeof(payload_pak_dir),
            rom_stub_path, sizeof(rom_stub_path),
            installed_tool_pak_dir, sizeof(installed_tool_pak_dir),
            installed_tool_launch_path, sizeof(installed_tool_launch_path),
            installed_tool_metadata_path, sizeof(installed_tool_metadata_path),
            manifest_path, sizeof(manifest_path)) != 0) {
        fprintf(stderr, "Could not derive install layout\n");
        return 1;
    }

    if (ap_init(&cfg) != AP_OK) {
        fprintf(stderr, "apostrophe init failed\n");
        return 1;
    }

    ap_log("startup: platform=%s tool_pak_dir=%s", platform_name(platform), tool_pak_dir);

    memset(&job, 0, sizeof(job));
    if (load_release_candidates(releases, MAX_REMOTE_RELEASES, &release_count) != 0 || release_count == 0) {
        ap_log("metadata load failed");
        show_message_box("Unable to load PortMaster release metadata from GitHub.");
        ap_quit();
        return 1;
    }
    latest_remote = releases[0];
    selected_remote = latest_remote;
    ap_log("metadata loaded: source=%s version=%s asset=%s",
        latest_remote.runtime_source, latest_remote.runtime_version, latest_remote.runtime_asset_name);

    load_install_state(layout.manifest_path, &installed);
    files_present = install_files_present(&layout);
    if (load_warning_consent(&layout, &consent) != 0) {
        memset(&consent, 0, sizeof(consent));
        ap_log("warning consent could not be loaded; treating as not accepted");
    }
    if (load_controller_layout(&layout, &current_controller_layout) != 0) {
        current_controller_layout = CONTROLLER_LAYOUT_X360;
        ap_log("controller layout could not be loaded; defaulting to X360");
    }
    for (;;) {
        model = resolve_status(installed, selected_remote, files_present);
        format_status_body(&model, &installed, &latest_remote, &selected_remote, selected_index != 0);
        ap_log("status resolved: action=%d files_present=%d installed=%s target=%s",
            model.action, files_present, installed.installed_runtime_version, selected_remote.runtime_version);

        {
            int ui_rc = run_updater_ui(&model, &updater_choice);
            updater_flow_decision decision;

            ap_log("updater ui returned: rc=%d choice=%d action=%d", ui_rc, updater_choice, model.action);
            if (ui_rc != AP_OK)
                decision = FLOW_DECISION_EXIT;
            else
                decision = decide_updater_flow(updater_choice, &model, &consent);

            if (decision == FLOW_DECISION_EXIT) {
                ap_quit();
                return 0;
            }

            if (decision == FLOW_DECISION_OPEN_SETTINGS) {
                int settings_index = selected_index;
                controller_layout settings_layout = current_controller_layout;

                if (show_settings_screen(&layout, releases, (int)release_count, selected_index, current_controller_layout,
                        &settings_index, &settings_layout) == AP_OK) {
                    if (save_controller_layout(&layout, settings_layout) != 0) {
                        show_message_box("Could not save controller layout.");
                    } else if (settings_index >= 0 && settings_index < (int)release_count) {
                        selected_index = settings_index;
                        selected_remote = releases[selected_index];
                        current_controller_layout = settings_layout;
                    }
                }
                continue;
            }

            if (decision == FLOW_DECISION_PROMPT_WARNING) {
                ui_warning_choice warning_choice = UI_WARNING_CANCEL;

                if (show_unsupported_warning(unsupported_warning_message(), &warning_choice) != AP_OK ||
                        warning_choice != UI_WARNING_ACCEPT) {
                    continue;
                }

                consent.accepted = 1;
                fill_iso8601_now(consent.accepted_at, sizeof(consent.accepted_at));
                if (save_warning_consent(&layout, &consent) != 0) {
                    memset(&consent, 0, sizeof(consent));
                    show_message_box("Could not save warning acknowledgement.");
                    continue;
                }
            }
        }
        break;
    }

    job.layout = layout;
    job.remote = selected_remote;
    snprintf(job.tool_pak_dir, sizeof(job.tool_pak_dir), "%s", tool_pak_dir);
    if (updater_choice == UI_UPDATER_FORCE_REINSTALL) {
        strncpy(job.status_line, "Preparing reinstall...", sizeof(job.status_line) - 1);
    } else {
        strncpy(job.status_line, "Preparing install...", sizeof(job.status_line) - 1);
    }
    job.dynamic_message = job.status_line;
    snprintf(job.target_state.installed_runtime_source, sizeof(job.target_state.installed_runtime_source), "%s", selected_remote.runtime_source);
    snprintf(job.target_state.installed_runtime_version, sizeof(job.target_state.installed_runtime_version), "%s", selected_remote.runtime_version);
    snprintf(job.target_state.installed_runtime_asset_name, sizeof(job.target_state.installed_runtime_asset_name), "%s", selected_remote.runtime_asset_name);
    snprintf(job.target_state.installed_runtime_asset_url, sizeof(job.target_state.installed_runtime_asset_url), "%s", selected_remote.runtime_asset_url);
    if (selected_remote.published_at[0] != '\0') {
        snprintf(job.target_state.installed_at, sizeof(job.target_state.installed_at), "%s", selected_remote.published_at);
    } else {
        fill_iso8601_now(job.target_state.installed_at, sizeof(job.target_state.installed_at));
    }
    fill_iso8601_now(job.target_state.last_checked_at, sizeof(job.target_state.last_checked_at));

    {
        int process_rc = show_process_dialog("Installing PortMaster runtime...", run_install_job, &job,
            &job.progress, &job.interrupt_signal, &job.dynamic_message);
        ap_log("process dialog returned: rc=%d interrupt=%d error=%s",
            process_rc, job.interrupt_signal, job.error);
        if (process_rc != AP_OK) {
            show_message_box("Install cancelled.");
            ap_quit();
            return 1;
        }
    }

    if (job.error[0] != '\0') {
        ap_log("install job error: %s", job.error);
        show_message_box(job.error);
        ap_quit();
        return 1;
    }

    ap_log("install job completed successfully");
    show_message_box("PortMaster runtime installed.");
    ap_quit();
    return 0;
}

static int derive_sdcard_root(const char *tool_pak_dir, char *sdcard_root, size_t sdcard_root_size) {
    const char *override = getenv("PMI_SDCARD_ROOT");
    const char *tools_marker = strstr(tool_pak_dir, "/Tools/");
    size_t prefix_len;

    if (override != NULL && override[0] != '\0') {
        if (snprintf(sdcard_root, sdcard_root_size, "%s", override) >= (int)sdcard_root_size)
            return -1;
        return 0;
    }

    if (tools_marker == NULL)
        return snprintf(sdcard_root, sdcard_root_size, "%s/build/dev-sdcard", tool_pak_dir) < (int)sdcard_root_size ? 0 : -1;

    prefix_len = (size_t)(tools_marker - tool_pak_dir);
    if (prefix_len + 1 > sdcard_root_size)
        return -1;

    memcpy(sdcard_root, tool_pak_dir, prefix_len);
    sdcard_root[prefix_len] = '\0';
    return 0;
}

static int build_install_layout(const char *tool_pak_dir, platform_id platform, install_layout *layout,
    char *sdcard_root, size_t sdcard_root_size,
    char *runtime_tools_dir, size_t runtime_tools_dir_size,
    char *payload_template_dir, size_t payload_template_dir_size,
    char *payload_pak_dir, size_t payload_pak_dir_size,
    char *rom_stub_path, size_t rom_stub_path_size,
    char *installed_tool_pak_dir, size_t installed_tool_pak_dir_size,
    char *installed_tool_launch_path, size_t installed_tool_launch_path_size,
    char *installed_tool_metadata_path, size_t installed_tool_metadata_path_size,
    char *manifest_path, size_t manifest_path_size) {
    const char *platform_value = platform_name(platform);

    if (derive_sdcard_root(tool_pak_dir, sdcard_root, sdcard_root_size) != 0)
        return -1;

    if (format_checked(payload_template_dir, payload_template_dir_size, "%s/payload/PORTS.pak", tool_pak_dir) != 0 ||
            format_checked(runtime_tools_dir, runtime_tools_dir_size, "%s/resources/runtime-bin/%s",
                tool_pak_dir, platform_value) != 0 ||
            format_checked(payload_pak_dir, payload_pak_dir_size, "%s/Emus/%s/PORTS.pak",
                sdcard_root, platform_value) != 0 ||
            format_checked(rom_stub_path, rom_stub_path_size, "%s/Roms/Ports (PORTS)/0) Portmaster.sh",
                sdcard_root) != 0 ||
            format_checked(installed_tool_pak_dir, installed_tool_pak_dir_size, "%s/Tools/%s/PortMaster.pak",
                sdcard_root, platform_value) != 0 ||
            format_checked(installed_tool_launch_path, installed_tool_launch_path_size, "%s/launch.sh",
                installed_tool_pak_dir) != 0 ||
            format_checked(installed_tool_metadata_path, installed_tool_metadata_path_size, "%s/pak.json",
                installed_tool_pak_dir) != 0 ||
            format_checked(manifest_path, manifest_path_size, "%s/.portmaster-installer.json", payload_pak_dir) != 0)
        return -1;

    layout->sdcard_root = sdcard_root;
    layout->tool_pak_dir = tool_pak_dir;
    layout->runtime_tools_dir = runtime_tools_dir;
    layout->payload_template_dir = payload_template_dir;
    layout->payload_pak_dir = payload_pak_dir;
    layout->rom_stub_path = rom_stub_path;
    layout->installed_tool_pak_dir = installed_tool_pak_dir;
    layout->installed_tool_launch_path = installed_tool_launch_path;
    layout->installed_tool_metadata_path = installed_tool_metadata_path;
    layout->manifest_path = manifest_path;
    layout->platform_name = platform_value;
    return 0;
}

static void fill_iso8601_now(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm tm_now;

    gmtime_r(&now, &tm_now);
    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", &tm_now);
}

static int format_checked(char *buffer, size_t buffer_size, const char *format, ...) {
    va_list args;
    int written;

    if (buffer == NULL || buffer_size == 0 || format == NULL)
        return -1;

    va_start(args, format);
    written = vsnprintf(buffer, buffer_size, format, args);
    va_end(args);

    return written >= 0 && (size_t)written < buffer_size ? 0 : -1;
}

static int load_release_candidates(remote_metadata *releases, size_t capacity, size_t *count) {
    char release_json[1048576];
    size_t index;

    if (releases == NULL || count == NULL || capacity == 0)
        return -1;

    if (http_get_text(PORTMASTER_RELEASES_URL, release_json, sizeof(release_json)) != 0)
        return -1;
    if (parse_release_candidates(release_json, PORTMASTER_RUNTIME_ASSET, releases, capacity, count) != 0)
        return -1;
    for (index = 0; index < *count; index++) {
        if (snprintf(releases[index].runtime_source, sizeof(releases[index].runtime_source), "%s",
                PORTMASTER_RUNTIME_SOURCE) >= (int)sizeof(releases[index].runtime_source))
            return -1;
    }

    return 0;
}

static void format_status_body(status_model *model, const install_state *installed,
    const remote_metadata *latest_remote, const remote_metadata *selected_remote, int selection_active) {
    const char *installed_version = installed->installed_runtime_version[0] != '\0'
        ? installed->installed_runtime_version : "not installed";

    if (model == NULL || latest_remote == NULL || selected_remote == NULL)
        return;

    if (!selection_active)
        return;

    switch (model->action) {
        case ACTION_INSTALL:
            snprintf(model->body, sizeof(model->body),
                "Installed: %.63s\nSelected: %.63s\nLatest: %.63s",
                installed_version,
                selected_remote->runtime_version,
                latest_remote->runtime_version);
            break;
        case ACTION_UPDATE:
            snprintf(model->body, sizeof(model->body),
                "Installed: %.63s\nSelected: %.63s\nLatest: %.63s",
                installed_version,
                selected_remote->runtime_version,
                latest_remote->runtime_version);
            break;
        case ACTION_NONE:
        default:
            snprintf(model->body, sizeof(model->body),
                "Installed: %.63s\nSelected: %.63s\nLatest: %.63s",
                installed_version,
                selected_remote->runtime_version,
                latest_remote->runtime_version);
            break;
    }
}

static int run_install_job(void *userdata) {
    install_job *job = userdata;
    char temp_template[] = "/tmp/porty-mcportface-XXXXXX";
    char *temp_dir = mkdtemp(temp_template);
    char archive_path[PATH_MAX];
    char extract_dir[PATH_MAX];
    char stage_root[PATH_MAX];

    if (temp_dir == NULL) {
        snprintf(job->error, sizeof(job->error), "Could not create temporary workspace.");
        return AP_ERROR;
    }

    if (format_checked(archive_path, sizeof(archive_path), "%s/%s",
            temp_dir, job->remote.runtime_asset_name) != 0 ||
            format_checked(extract_dir, sizeof(extract_dir), "%s/extract", temp_dir) != 0 ||
            format_checked(stage_root, sizeof(stage_root), "%s/Apps/PortMaster", extract_dir) != 0) {
        snprintf(job->error, sizeof(job->error), "Temporary path is too long.");
        return AP_ERROR;
    }

    snprintf(job->status_line, sizeof(job->status_line), "Downloading upstream TrimUI runtime...");
    job->progress = 0.25f;
    if (http_download_file(job->remote.runtime_asset_url, archive_path) != 0) {
        snprintf(job->error, sizeof(job->error), "Runtime download failed.");
        return AP_ERROR;
    }

    if (job->interrupt_signal) {
        snprintf(job->error, sizeof(job->error), "Install cancelled.");
        return AP_CANCELLED;
    }

    snprintf(job->status_line, sizeof(job->status_line), "Extracting runtime...");
    job->progress = 0.55f;
    if (extract_stage_archive(job->tool_pak_dir, archive_path, extract_dir) != 0) {
        snprintf(job->error, sizeof(job->error), "Archive extraction failed.");
        return AP_ERROR;
    }

    if (job->interrupt_signal) {
        snprintf(job->error, sizeof(job->error), "Install cancelled.");
        return AP_CANCELLED;
    }

    snprintf(job->status_line, sizeof(job->status_line), "Installing payload...");
    job->progress = 0.85f;
    if (install_from_stage(stage_root, &job->layout, &job->target_state) != 0) {
        snprintf(job->error, sizeof(job->error), "Install step failed.");
        return AP_ERROR;
    }

    snprintf(job->status_line, sizeof(job->status_line), "Done.");
    job->progress = 1.0f;
    return AP_OK;
}
