#include "status.h"

#include <stdio.h>
#include <string.h>

status_model resolve_status(install_state installed, remote_metadata remote, int files_present) {
    status_model model;
    int missing_install_metadata;
    int runtime_changed;

    memset(&model, 0, sizeof(model));
    snprintf(model.title, sizeof(model.title), "Porty McPortface");

    if (remote.runtime_version[0] == '\0' || remote.runtime_asset_name[0] == '\0') {
        model.action = ACTION_INSTALL;
        snprintf(model.body, sizeof(model.body), "Remote PortMaster release metadata could not be loaded.");
        return model;
    }

    missing_install_metadata = installed.installed_runtime_source[0] == '\0' ||
        installed.installed_runtime_version[0] == '\0' ||
        installed.installed_runtime_asset_name[0] == '\0';

    if (!files_present || missing_install_metadata) {
        model.action = ACTION_INSTALL;
        snprintf(model.body, sizeof(model.body), "PortMaster %s is not installed.", remote.runtime_version);
        return model;
    }

    runtime_changed = strcmp(installed.installed_runtime_source, remote.runtime_source) != 0 ||
        strcmp(installed.installed_runtime_version, remote.runtime_version) != 0 ||
        strcmp(installed.installed_runtime_asset_name, remote.runtime_asset_name) != 0;

    if (runtime_changed) {
        model.action = ACTION_UPDATE;
        snprintf(model.body, sizeof(model.body),
            "Installed: %.63s\nLatest: %.63s\nAsset: %.96s",
            installed.installed_runtime_version,
            remote.runtime_version,
            remote.runtime_asset_name);
        return model;
    }

    model.action = ACTION_NONE;
    snprintf(model.body, sizeof(model.body), "Installed runtime matches the latest upstream TrimUI release.");
    return model;
}
