#ifndef INSTALL_H
#define INSTALL_H

#include "json.h"

typedef struct {
    const char *sdcard_root;
    const char *tool_pak_dir;
    const char *runtime_tools_dir;
    const char *payload_template_dir;
    const char *payload_pak_dir;
    const char *rom_stub_path;
    const char *installed_tool_pak_dir;
    const char *installed_tool_launch_path;
    const char *installed_tool_metadata_path;
    const char *manifest_path;
    const char *platform_name;
} install_layout;

int install_from_stage(const char *stage_root, const install_layout *layout, const install_state *state);
int extract_stage_archive(const char *tool_pak_dir, const char *archive_path, const char *extract_dir);
int load_install_state(const char *manifest_path, install_state *out);
int install_files_present(const install_layout *layout);

#endif
