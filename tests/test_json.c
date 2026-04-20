#include <assert.h>
#include <string.h>

#include "../src/json.h"

int main(void) {
    const char *release_json =
        "["
        "{"
        "\"tag_name\":\"2026.04.05-0000\","
        "\"draft\":false,"
        "\"prerelease\":true,"
        "\"published_at\":\"2026-04-05T00:00:00Z\","
        "\"assets\":["
        "{\"name\":\"trimui.portmaster.zip\",\"size\":999,\"browser_download_url\":\"https://example.com/prerelease.zip\"}"
        "]"
        "},"
        "{"
        "\"tag_name\":\"2026.04.01-1426\","
        "\"draft\":false,"
        "\"prerelease\":false,"
        "\"published_at\":\"2026-04-01T14:26:00Z\","
        "\"assets\":["
        "{\"name\":\"PortMaster.zip\",\"size\":123,\"browser_download_url\":\"https://example.com/PortMaster.zip\"},"
        "{\"name\":\"trimui.portmaster.zip\",\"size\":456,\"browser_download_url\":\"https://example.com/trimui.portmaster.zip\"}"
        "]"
        "},"
        "{"
        "\"tag_name\":\"2026.03.15-0100\","
        "\"draft\":false,"
        "\"prerelease\":false,"
        "\"published_at\":\"2026-03-15T01:00:00Z\","
        "\"assets\":["
        "{\"name\":\"other.zip\",\"size\":222,\"browser_download_url\":\"https://example.com/other.zip\"}"
        "]"
        "},"
        "{"
        "\"tag_name\":\"2026.03.01-1200\","
        "\"draft\":false,"
        "\"prerelease\":false,"
        "\"published_at\":\"2026-03-01T12:00:00Z\","
        "\"assets\":["
        "{\"name\":\"trimui.portmaster.zip\",\"size\":333,\"browser_download_url\":\"https://example.com/trimui-older.zip\"}"
        "]"
        "}"
        "]";
    const char *manifest_json =
        "{"
        "\"installed_runtime_source\":\"portmaster_gui_trimui\","
        "\"installed_runtime_version\":\"2026.04.01-1426\","
        "\"installed_runtime_asset_name\":\"trimui.portmaster.zip\","
        "\"installed_runtime_asset_url\":\"https://example.com/trimui.portmaster.zip\","
        "\"installed_at\":\"2026-04-01T14:26:00Z\","
        "\"last_checked_at\":\"2026-04-14T21:00:00Z\""
        "}";
    remote_metadata releases[MAX_REMOTE_RELEASES];
    install_state state;
    char output[4096];
    size_t release_count = 0;

    assert(parse_release_candidates(release_json, "trimui.portmaster.zip", releases, MAX_REMOTE_RELEASES, &release_count) == 0);
    assert(release_count == 2);
    assert(strcmp(releases[0].runtime_version, "2026.04.01-1426") == 0);
    assert(strcmp(releases[0].runtime_asset_name, "trimui.portmaster.zip") == 0);
    assert(strcmp(releases[0].runtime_asset_url, "https://example.com/trimui.portmaster.zip") == 0);
    assert(strcmp(releases[0].published_at, "2026-04-01T14:26:00Z") == 0);
    assert(releases[0].size_bytes == 456);
    assert(strcmp(releases[1].runtime_version, "2026.03.01-1200") == 0);
    assert(strcmp(releases[1].runtime_asset_url, "https://example.com/trimui-older.zip") == 0);

    assert(parse_install_state(manifest_json, &state) == 0);
    assert(strcmp(state.installed_runtime_source, "portmaster_gui_trimui") == 0);
    assert(strcmp(state.installed_runtime_version, "2026.04.01-1426") == 0);
    assert(strcmp(state.installed_runtime_asset_name, "trimui.portmaster.zip") == 0);
    assert(strcmp(state.installed_runtime_asset_url, "https://example.com/trimui.portmaster.zip") == 0);
    assert(format_install_state_json(&state, output, sizeof(output)) == 0);
    assert(strstr(output, "\"installed_runtime_source\": \"portmaster_gui_trimui\"") != NULL);
    assert(strstr(output, "\"installed_runtime_version\": \"2026.04.01-1426\"") != NULL);
    assert(strstr(output, "\"installed_runtime_asset_name\": \"trimui.portmaster.zip\"") != NULL);

    return 0;
}
