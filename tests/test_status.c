#include <assert.h>
#include <string.h>

#include "../src/status.h"

int main(void) {
    remote_metadata latest = {0};
    remote_metadata selected = {0};
    install_state installed = {0};

    strcpy(latest.runtime_source, "portmaster_gui_trimui");
    strcpy(latest.runtime_version, "2026.04.01-1426");
    strcpy(latest.runtime_asset_name, "trimui.portmaster.zip");
    strcpy(selected.runtime_source, "portmaster_gui_trimui");
    strcpy(selected.runtime_version, "2025.03.22-1319");
    strcpy(selected.runtime_asset_name, "trimui.portmaster.zip");

    {
        status_model fresh = resolve_status(installed, latest, 0);
        assert(fresh.action == ACTION_INSTALL);
        assert(strstr(fresh.body, "not installed") != NULL);
    }

    strcpy(installed.installed_runtime_source, "portmaster_gui_trimui");
    strcpy(installed.installed_runtime_version, "2025.03.22-1319");
    strcpy(installed.installed_runtime_asset_name, "trimui.portmaster.zip");
    {
        status_model update = resolve_status(installed, latest, 1);
        assert(update.action == ACTION_UPDATE);
        assert(strstr(update.body, "Latest") != NULL);
    }

    {
        status_model current = resolve_status(installed, selected, 1);
        assert(current.action == ACTION_NONE);
    }

    strcpy(installed.installed_runtime_version, "2026.04.01-1426");
    {
        status_model back_to_latest = resolve_status(installed, latest, 1);
        assert(back_to_latest.action == ACTION_NONE);
    }

    return 0;
}
