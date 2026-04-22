#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/fs.h"
#include "../src/install.h"

static void join_path(char *buffer, size_t buffer_size, const char *root, const char *suffix) {
    snprintf(buffer, buffer_size, "%s/%s", root, suffix);
}

int main(void) {
    char root_template[] = "/tmp/pm-installer-test-XXXXXX";
    char *root = mkdtemp(root_template);
    char repo_root[PATH_MAX];
    char stage_root[PATH_MAX];
    char runtime_tools_dir[PATH_MAX];
    char payload_dir[PATH_MAX];
    char rom_stub[PATH_MAX];
    char tool_pak_dir[PATH_MAX];
    char tool_launch[PATH_MAX];
    char tool_metadata[PATH_MAX];
    char manifest_path[PATH_MAX];
    char file_path[PATH_MAX];
    char expected_runtime_dir[PATH_MAX];
    char expected_temp_dir[PATH_MAX];
    char *file_content = NULL;
    size_t file_len = 0;
    struct stat st = {0};
    install_layout layout;
    install_state state = {0};

    assert(root != NULL);
    assert(getcwd(repo_root, sizeof(repo_root)) != NULL);

    join_path(stage_root, sizeof(stage_root), root, "stage/Apps/PortMaster");
    join_path(runtime_tools_dir, sizeof(runtime_tools_dir), root, "runtime-bin");
    join_path(payload_dir, sizeof(payload_dir), root, "Emus/my355/PORTS.pak");
    join_path(rom_stub, sizeof(rom_stub), root, "Roms/Ports (PORTS)/0) Portmaster.sh");
    join_path(tool_pak_dir, sizeof(tool_pak_dir), root, "Tools/my355/PortMaster.pak");
    join_path(tool_launch, sizeof(tool_launch), tool_pak_dir, "launch.sh");
    join_path(tool_metadata, sizeof(tool_metadata), tool_pak_dir, "pak.json");
    join_path(manifest_path, sizeof(manifest_path), root, "Emus/my355/PORTS.pak/.portmaster-installer.json");
    join_path(expected_runtime_dir, sizeof(expected_runtime_dir), root, "Emus/my355/PORTS.pak/PortMaster");
    join_path(expected_temp_dir, sizeof(expected_temp_dir), root, ".ports_temp");

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/runtime.txt", stage_root);
    assert(write_text_file(file_path, "runtime") == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pugwash", stage_root);
    assert(write_text_file(file_path,
        "#!/usr/bin/env python3\n"
        "def portmaster_check_update(pm, config, temp_dir):\n"
        "    print('updating')\n"
        "\n"
        "class HarbourMaster:\n"
        "    pass\n"
        "\n"
        "class PortMasterGUI:\n"
        "    def __init__(self):\n"
        "        self.hm = None\n"
        "        self.images = None\n"
        "        self.text_data = {}\n"
        "        self.changed_keys = set()\n"
        "        self.timers = None\n"
        "        self.dir_scanner = None\n"
        "\n"
        "    def set_data(self, key, value):\n"
        "        self.text_data[key] = value\n"
        "        self.changed_keys.add(key)\n"
        "\n"
        "    def get_port_image(self, port_name):\n"
        "        image = None\n"
        "        if self.hm is not None:\n"
        "            image = self.hm.port_images(port_name)\n"
        "\n"
        "            if image is not None:\n"
        "                image = image.get('screenshot', None)\n"
        "\n"
        "        if image is None:\n"
        "            image = \"NO_IMAGE\"\n"
        "\n"
        "        return image\n"
        "\n"
        "    def set_port_info(self, port_name, port_info, want_install_size=False):\n"
        "        return None\n"
        "\n"
        "    def do_update(self):\n"
        "        # Update scanning\n"
        "        if self.timers.elapsed('dir_scan_interval', 500, run_first=True):\n"
        "            self.dir_scanner.iterate(30)\n"
        "\n"
        "        ## Check for any keys changed in our template system.\n"
        "        if len(self.changed_keys):\n"
        "            self.changed_keys.clear()\n"
        "\n"
        "def create_pm(config, temp_dir, argv):\n"
        "    pm = PortMasterGUI()\n"
        "    if len(argv) > 1 and argv[1] == 'fifo_control':\n"
        "            pm.hm = HarbourMaster(config, temp_dir=temp_dir, callback=pm)\n"
        "    with pm.enable_cancellable(False):\n"
        "            pm.hm = HarbourMaster(config, temp_dir=temp_dir, callback=pm)\n"
        "            pm.hm = HarbourMaster(config, temp_dir=temp_dir, callback=pm)\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/harbourmaster/platform.py", stage_root);
    assert(write_text_file(file_path,
        "import os\n"
        "from pathlib import Path\n"
        "CONTROL_HACK = Path(\"/roms/ports/PortMaster/control.txt\")\n"
        "CONTROL_HACK = Path(\"/root/.local/share/PortMaster/control.txt\")\n"
        "ROM_SCRIPT_DIR = Path(\"/mnt/SDCARD/Roms/PORTS\")\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/harbourmaster/hardware.py", stage_root);
    assert(write_text_file(file_path,
        "from pathlib import Path\n"
        "import os\n"
        "HM_TESTING=False\n"
        "\n"
        "def cpu_info_v2(info):\n"
        "    if Path('/lib/ld-linux-armhf.so.3').exists():\n"
        "        info[\"capabilities\"].append(\"armhf\")\n"
        "        info['primary_arch'] = \"armhf\"\n"
        "\n"
        "    if Path('/lib/ld-linux-aarch64.so.1').exists():\n"
        "        info[\"capabilities\"].append(\"aarch64\")\n"
        "        info['primary_arch'] = \"aarch64\"\n"
        "\n"
        "    if Path('/lib/ld-linux.so.2').exists():\n"
        "        info[\"capabilities\"].append(\"x86\")\n"
        "        info['primary_arch'] = \"x86\"\n"
        "\n"
        "    if (\n"
        "            Path('/lib/ld-linux-x86-64.so.2').exists() or\n"
        "            Path('/lib64/ld-linux-x86-64.so.2').exists() or\n"
        "            Path('/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2').exists()):\n"
        "        info[\"capabilities\"].append(\"x86_64\")\n"
        "        info['primary_arch'] = \"x86_64\"\n"
        "\n"
        "    if HM_TESTING or 'primary_arch' not in info:\n"
        "        info[\"capabilities\"].append(\"armhf\")\n"
        "        info[\"capabilities\"].append(\"aarch64\")\n"
        "        info['primary_arch'] = \"aarch64\"\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/default_theme/theme.json", stage_root);
    assert(write_text_file(file_path,
        "{\n"
        "    \"#schemes\": {\n"
        "        \"#base\": {\n"
        "            \"#resources\": {\n"
        "                \"no-image.jpg\": {\n"
        "                    \"name\": \"NO_IMAGE\"\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/pySDL2gui.py", stage_root);
    assert(write_text_file(file_path,
        "import os\n"
        "from pathlib import Path\n"
        "\n"
        "class Image:\n"
        "    pass\n"
        "\n"
        "class ImageManager:\n"
        "    def __init__(self, gui, max_images=None):\n"
        "        self.gui = gui\n"
        "        self.renderer = gui.renderer\n"
        "        self.images = {}\n"
        "        self.textures = {}\n"
        "        self.cache = []\n"
        "\n"
        "    def load(self, filename):\n"
        "        if filename in self.cache:\n"
        "            return self.images[filename]\n"
        "        else:\n"
        "            res_filename = self.gui.resources.find(filename)\n"
        "\n"
        "            if res_filename is None:\n"
        "                return None\n"
        "\n"
        "            surf = sdl2.ext.image.load_img(res_filename)\n"
        "\n"
        "            texture = sdl2.ext.renderer.Texture(self.renderer, surf)\n"
        "\n"
        "            sdl2.SDL_FreeSurface(surf)\n"
        "\n"
        "            self.textures[filename] = texture\n"
        "            self.images[filename] = Image(texture, renderer=self.renderer)\n"
        "            self.cache.insert(0, filename)\n"
        "\n"
        "            return self.images[filename]\n"
        "\n"
        "    def load_data(self, file_name, data):\n"
        "        res_filename = self.gui.resources.find(file_name)\n"
        "\n"
        "        if res_filename is None:\n"
        "            return None\n"
        "\n"
        "        if file_name.lower().endswith('.svg') and \"size\" in data:\n"
        "            image_size = data[\"size\"]\n"
        "\n"
        "            if len(image_size) != 2:\n"
        "                return None\n"
        "\n"
        "            if not isinstance(image_size[0], int) or not isinstance(image_size[1], int):\n"
        "                return None\n"
        "\n"
        "            surf = sdl2.ext.image.load_svg(res_filename, width=image_size[0], height=image_size[1])\n"
        "        else:\n"
        "            surf = sdl2.ext.image.load_img(res_filename)\n"
        "\n"
        "        texture = sdl2.ext.renderer.Texture(self.renderer, surf)\n"
        "        sdl2.SDL_FreeSurface(surf)\n"
        "\n"
        "        stored_name = data.get(\"name\", file_name)\n"
        "        image_mod = data.get(\"image-mod\", None)\n"
        "\n"
        "        if \"atlas\" in data:\n"
        "            return {}\n"
        "        else:\n"
        "            image = Image(texture, renderer=self.renderer, color_mod=image_mod)\n"
        "            self.images[stored_name] = image\n"
        "            return image\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/pm-artwork-convert", runtime_tools_dir);
    assert(write_text_file(file_path, "#!/bin/sh\nexit 0\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/pm-sdl-compat-check", runtime_tools_dir);
    assert(write_text_file(file_path, "#!/bin/sh\nexit 0\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/pm-port-probe", runtime_tools_dir);
    assert(write_text_file(file_path, "#!/bin/sh\nexit 0\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/pm-power-lid-watch", runtime_tools_dir);
    assert(write_text_file(file_path, "#!/bin/sh\nexit 0\n") == 0);
    snprintf(file_path, sizeof(file_path), "%s/box64", runtime_tools_dir);
    assert(write_text_file(file_path, "#!/bin/sh\nexit 0\n") == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "armhf");
    assert(fs_ensure_dir(file_path) == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "armhf/miyoo355_rootfs_32.img_partaa");
    assert(write_text_file(file_path, "rootfs-aa") == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "armhf/miyoo355_rootfs_32.img_partab");
    assert(write_text_file(file_path, "rootfs-ab") == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "armhf/miyoo355_rootfs_32.img_partac");
    assert(write_text_file(file_path, "rootfs-ac") == 0);
    snprintf(file_path, sizeof(file_path), "%s/box64-i386-linux-gnu", runtime_tools_dir);
    assert(fs_ensure_dir(file_path) == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "box64-i386-linux-gnu/libstdc++.so.6");
    assert(write_text_file(file_path, "i386-libstdc++") == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "box64-i386-linux-gnu/libgcc_s.so.1");
    assert(write_text_file(file_path, "i386-libgcc") == 0);
    snprintf(file_path, sizeof(file_path), "%s/box64-x86_64-linux-gnu", runtime_tools_dir);
    assert(fs_ensure_dir(file_path) == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "box64-x86_64-linux-gnu/libc.so.6");
    assert(write_text_file(file_path, "x64-libc") == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "aarch64");
    assert(fs_ensure_dir(file_path) == 0);
    join_path(file_path, sizeof(file_path), runtime_tools_dir, "aarch64/libSDL2-2.0.so.0");
    assert(write_text_file(file_path, "aarch64-sdl2") == 0);
    assert(write_text_file(tool_launch, "#!/bin/sh\nexit 0\n") == 0);
    assert(write_text_file(tool_metadata, "{}\n") == 0);

    strcpy(state.installed_runtime_source, "portmaster_gui_trimui");
    strcpy(state.installed_runtime_version, "2026.04.01-1426");
    strcpy(state.installed_runtime_asset_name, "trimui.portmaster.zip");
    strcpy(state.installed_runtime_asset_url, "https://example.com/trimui.portmaster.zip");
    strcpy(state.installed_at, "2026-04-01T14:26:00Z");
    strcpy(state.last_checked_at, "2026-04-14T21:00:00Z");

    layout.sdcard_root = root;
    layout.tool_pak_dir = repo_root;
    layout.runtime_tools_dir = runtime_tools_dir;
    layout.payload_template_dir = "payload/PORTS.pak";
    layout.payload_pak_dir = payload_dir;
    layout.rom_stub_path = rom_stub;
    layout.installed_tool_pak_dir = tool_pak_dir;
    layout.installed_tool_launch_path = tool_launch;
    layout.installed_tool_metadata_path = tool_metadata;
    layout.manifest_path = manifest_path;
    layout.platform_name = "my355";

    assert(install_from_stage(stage_root, &layout, &state) == 0);
    assert(stat(layout.rom_stub_path, &st) == 0);
    assert(stat(layout.manifest_path, &st) == 0);
    assert(stat(layout.installed_tool_launch_path, &st) != 0);
    assert(stat(layout.installed_tool_metadata_path, &st) != 0);

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/runtime.txt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/config.json", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/PortMaster.txt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/config.py", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/gamecontrollerdb.txt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/bin.tar.gz", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/lib.tar.gz", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/libmali-g2p0.so.1.9.0", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/ca-certificates.crt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/files/libffi.so.7", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/control.txt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/miyoo/control.txt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/PortMaster.txt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/PortMaster.sh", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/miyoo/PortMaster.txt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/harbourmaster/config.py", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/PortMaster/gamecontrollerdb.txt", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/bin/pm-artwork-convert", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/bin/pm-sdl-compat-check", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/bin/pm-port-probe", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/bin/pm-power-lid-watch", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/bin/box64", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/runtime/armhf/miyoo355_rootfs_32.img_partaa", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/runtime/armhf/miyoo355_rootfs_32.img_partab", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/runtime/armhf/miyoo355_rootfs_32.img_partac", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/runtime/aarch64/lib/libSDL2-2.0.so.0", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/lib/box64-i386-linux-gnu/libstdc++.so.6", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/lib/box64-i386-linux-gnu/libgcc_s.so.1", payload_dir);
    assert(stat(file_path, &st) == 0);
    snprintf(file_path, sizeof(file_path), "%s/lib/box64-x86_64-linux-gnu/libc.so.6", payload_dir);
    assert(stat(file_path, &st) == 0);

    snprintf(file_path, sizeof(file_path), "%s/launch.sh", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "MOUNTED_PORTS_DIR=\"$TEMP_DATA_DIR/ports\"") != NULL);
    assert(strstr(file_content, "export HM_PORTS_DIR=\"$MOUNTED_PORTS_DIR\"") != NULL);
    assert(strstr(file_content, "export PMI_ROM_ROOT=\"$ROM_DIR\"") != NULL);
    assert(strstr(file_content, "export PMI_ARTWORK_CONVERTER=\"$PAK_DIR/bin/pm-artwork-convert\"") != NULL);
    assert(strstr(file_content, "export PATH=\"$EMU_DIR:$PAK_DIR/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH\"") != NULL);
    assert(strstr(file_content, "export LD_LIBRARY_PATH=\"/usr/trimui/lib:$EMU_DIR/runtimes/love_11.5/libs.aarch64:$PAK_DIR/lib") != NULL);
    assert(strstr(file_content, "PMI_HOME_ROOT=\"$USERDATA_PATH/PORTS-portmaster\"") != NULL);
    assert(strstr(file_content, "PMI_HOME_ROOT=\"$SDCARD_ROOT/.userdata/$PLATFORM/PORTS-portmaster\"") != NULL);
    assert(strstr(file_content, "HOME=\"$PMI_HOME_ROOT\"") != NULL);
    assert(strstr(file_content, "$XDG_DATA_HOME/PortMaster") == NULL);
    assert(strstr(file_content, "PMI_LOG_FILE=\"$USERDATA_PATH/logs/PORTS.txt\"") != NULL);
    assert(strstr(file_content, "PMI_LOG_FILE=\"$SDCARD_ROOT/.userdata/$PLATFORM/logs/PORTS.txt\"") != NULL);
    assert(strstr(file_content, "export PMI_LOG_FILE") != NULL);
    assert(strstr(file_content, "export PMI_HOME_ROOT") != NULL);
    assert(strstr(file_content, "export PYSDL2_DLL_PATH=\"/usr/lib:/usr/trimui/lib:$PAK_DIR/lib\"") != NULL);
    assert(strstr(file_content, "love_11.5/libs.aarch64") != NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/files/control.txt", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, expected_runtime_dir) != NULL);
    assert(strstr(file_content, "export controlfolder=\"$runtime_root\"") != NULL);
    assert(strstr(file_content, "runtime_root=\"") != NULL);
    assert(strstr(file_content, expected_temp_dir + 1) != NULL);
    assert(strstr(file_content, "get_controls()") != NULL);
    assert(strstr(file_content, "resolve_controller_mapping()") == NULL);
    assert(strstr(file_content, "log_control_diagnostics()") == NULL);
    assert(strstr(file_content, "export PATH=\"${runtime_root%/PortMaster}/bin:$PATH\"") != NULL);
    assert(strstr(file_content, "if [ \"${PMI_LD_LIBRARY_STRATEGY:-}\" = \"system-gl\" ]; then") != NULL);
    assert(strstr(file_content, "export LD_LIBRARY_PATH=\"/usr/lib:/usr/trimui/lib\"") != NULL);
    assert(strstr(file_content, "export LD_LIBRARY_PATH=\"${runtime_root%/PortMaster}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"") != NULL);
    assert(strstr(file_content, "SDL_GAMECONTROLLERCONFIG=") == NULL);
    assert(strstr(file_content, "source \"$controlfolder/device_info.txt\"") != NULL);
    assert(strstr(file_content, "source \"$controlfolder/funcs.txt\"") != NULL);
    assert(strstr(file_content, "export GPTOKEYB2=") != NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/Portmaster.sh", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "mount -o bind \"$REAL_PORTS_DIR\" \"$MOUNTED_PORTS_DIR\"") != NULL);
    assert(strstr(file_content, ": \"${PM_RUNTIME_ROOT:=$EMU_DIR}\"") != NULL);
    assert(strstr(file_content, "LOG_FILE=\"${PMI_LOG_FILE:-$PM_RUNTIME_ROOT/log.txt}\"") != NULL);
    assert(strstr(file_content, "SPRUCE_ARMHF_ROOTFS_MOUNT=\"$TEMP_DATA_DIR/spruceflip-rootfs\"") != NULL);
    assert(strstr(file_content, "if mount | grep -q \"on $SPRUCE_ARMHF_ROOTFS_MOUNT \"; then") != NULL);
    assert(strstr(file_content, "ensure_runtime_parity_overlay()") != NULL);
    assert(strstr(file_content, "process_runtime_autoinstall()") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/control.txt\" \"$PM_RUNTIME_ROOT/control.txt\"") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/control.txt\" \"$PM_RUNTIME_ROOT/miyoo/control.txt\"") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/PortMaster.txt\" \"$PM_RUNTIME_ROOT/PortMaster.txt\"") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/PortMaster.txt\" \"$PM_RUNTIME_ROOT/PortMaster.sh\"") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/PortMaster.txt\" \"$PM_RUNTIME_ROOT/miyoo/PortMaster.txt\"") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/config.py\" \"$PM_RUNTIME_ROOT/pylibs/harbourmaster/config.py\"") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/gamecontrollerdb.txt\" \"$PM_RUNTIME_ROOT/gamecontrollerdb.txt\"") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/control.txt\" \"$xdg_pm_dir/control.txt\"") != NULL);
    assert(strstr(file_content, "sync_overlay_file \"$PAK_DIR/files/gamecontrollerdb.txt\" \"$xdg_pm_dir/gamecontrollerdb.txt\"") != NULL);
    assert(strstr(file_content, "process_runtime_autoinstall_dir \"$PM_RUNTIME_ROOT/autoinstall\"") != NULL);
    assert(strstr(file_content, "process_runtime_autoinstall_dir \"$XDG_DATA_HOME/PortMaster/autoinstall\"") != NULL);
    assert(strstr(file_content, "PMI_DIAG overlay_controller_db=$PM_RUNTIME_ROOT/gamecontrollerdb.txt") != NULL);
    assert(strstr(file_content, "unpack_tar \"$PAK_DIR/files/bin.tar.gz\" \"$PAK_DIR/bin\"") != NULL);
    assert(strstr(file_content, "unpack_tar \"$PAK_DIR/files/lib.tar.gz\" \"$PAK_DIR/lib\"") != NULL);
    assert(strstr(file_content, "cp -f \"$PAK_DIR/files/libffi.so.7\" \"$PAK_DIR/lib/libffi.so.7\"") != NULL);
    assert(strstr(file_content, "if ! command -v pkill >/dev/null 2>&1; then") != NULL);
    assert(strstr(file_content, "cat >\"$PAK_DIR/bin/pkill\" <<'EOF'") != NULL);
    assert(strstr(file_content, "SPRUCE_ARMHF_ROOTFS_IMAGE=\"$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img\"") != NULL);
    assert(strstr(file_content, "SPRUCE_ARMHF_ROOTFS_PARTAA=\"$PAK_DIR/runtime/armhf/miyoo355_rootfs_32.img_partaa\"") != NULL);
    assert(strstr(file_content, "X86_LIB_PATH=\"$PAK_DIR/lib/box64-i386-linux-gnu\"") != NULL);
    assert(strstr(file_content, "mount -t squashfs \"$SPRUCE_ARMHF_ROOTFS_IMAGE\" \"$SPRUCE_ARMHF_ROOTFS_MOUNT\" >/dev/null 2>&1 || true") != NULL);
    assert(strstr(file_content, "ARMHF_ROOT=\"$SPRUCE_ARMHF_ROOTFS_MOUNT\"") != NULL);
    assert(strstr(file_content, "ARMHF_LIB_PATH=\"$ARMHF_ROOT/usr/lib\"") != NULL);
    assert(strstr(file_content, "export BOX86_PATH=\"$X86_LIB_PATH${BOX86_PATH:+:$BOX86_PATH}\"") != NULL);
    assert(strstr(file_content, "export LD_LIBRARY_PATH=\"$ARMHF_LIB_PATH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"") != NULL);
    assert(strstr(file_content, "write_box86_compat_wrapper()") != NULL);
    assert(strstr(file_content, "original_path=\"$wrapper_dir/box86.original\"") != NULL);
    assert(strstr(file_content, "REAL_BOX86=\"$SELF_DIR/box86.original\"") != NULL);
    assert(strstr(file_content, "ARMHF_LOADER=\"$ARMHF_ROOT/usr/lib/ld-linux-armhf.so.3\"") != NULL);
    assert(strstr(file_content, "export BOX86_LD_LIBRARY_PATH=\"$ARMHF_LIB_PATH${BOX86_LD_LIBRARY_PATH:+:$BOX86_LD_LIBRARY_PATH}\"") != NULL);
    assert(strstr(file_content, "exec \"$ARMHF_LOADER\" --library-path \"$ARMHF_LIB_PATH\" \"$REAL_BOX86\" \"$@\"") != NULL);
    assert(strstr(file_content, "write_armhf_exec_compat_wrapper()") != NULL);
    assert(strstr(file_content, "original_path=\"$wrapper_dir/${wrapper_name}.original\"") != NULL);
    assert(strstr(file_content, "REAL_BINARY=\"$SELF_DIR/${SELF_NAME}.original\"") != NULL);
    assert(strstr(file_content, "ARMHF_EXTRA_LIB_PATH=\"$PAK_DIR/runtime/armhf/lib\"") != NULL);
    assert(strstr(file_content, "for path_entry in ${LD_LIBRARY_PATH:-}; do") != NULL);
    assert(strstr(file_content, "\"$SELF_DIR\"|\"$SELF_DIR\"/*)") != NULL);
    assert(strstr(file_content, "ARMHF_EFFECTIVE_LD_LIBRARY_PATH=\"$ARMHF_LIB_PATH${ARMHF_PORT_LIB_PATH:+:$ARMHF_PORT_LIB_PATH}\"") != NULL);
    assert(strstr(file_content, "exec \"$ARMHF_LOADER\" --library-path \"$ARMHF_EFFECTIVE_LD_LIBRARY_PATH\" \"$REAL_BINARY\" \"$@\"") != NULL);
    assert(strstr(file_content, "default_sdl2_candidate_path()") != NULL);
    assert(strstr(file_content, "if [ -n \"${PMI_SDL2_SYSTEM_LIB:-}\" ] && [ -f \"$PMI_SDL2_SYSTEM_LIB\" ]; then") != NULL);
    assert(strstr(file_content, "strings \"$system_sdl\" 2>/dev/null | grep -qx 'SDL_GetDefaultAudioInfo'") != NULL);
    assert(strstr(file_content, "port_probe_helper_path()") != NULL);
    assert(strstr(file_content, "port_probe_cache_path()") != NULL);
    assert(strstr(file_content, "cleanup_legacy_port_probe_artifacts()") != NULL);
    assert(strstr(file_content, "refresh_port_probe_cache()") != NULL);
    assert(strstr(file_content, "launcher_aarch64_port_dir()") != NULL);
    assert(strstr(file_content, "printf '%s/.pmi-port-probe-v1.tsv\\n' \"$port_dir\"") != NULL);
    assert(strstr(file_content, "helper=$(port_probe_helper_path)") != NULL);
    assert(strstr(file_content, "\"$helper\" scan-aarch64-launch-port \"$port_dir\" >\"$tmp_cache\"") != NULL);
    assert(strstr(file_content, "echo \"PMI_WARN port_probe_helper_missing=$helper\"") != NULL);
    assert(strstr(file_content, "PMI_POWER_LID_HELPER_PIDFILE=\"/tmp/pmi-power-lid-watch.pid\"") != NULL);
    assert(strstr(file_content, "power_lid_helper_path()") != NULL);
    assert(strstr(file_content, "stop_power_lid_helper()") != NULL);
    assert(strstr(file_content, "start_power_lid_helper()") != NULL);
    assert(strstr(file_content, "helper=$(power_lid_helper_path)") != NULL);
    assert(strstr(file_content, "echo \"PMI_WARN power_lid_helper_missing=$helper\"") != NULL);
    assert(strstr(file_content, "\"$helper\" &") != NULL);
    assert(strstr(file_content, "echo \"PMI_DIAG power_lid_helper_started=$pid\"") != NULL);
    assert(strstr(file_content, "echo \"PMI_DIAG power_lid_helper_stop=$pid\"") != NULL);
    assert(strstr(file_content, "find \"$port_dir\" -maxdepth 2 -type f -newer \"$cache_path\"") != NULL);
    assert(strstr(file_content, "find \"$port_dir/lib\" -maxdepth 2 -type f") != NULL);
    assert(strstr(file_content, "maybe_refresh_aarch64_sdl_compat_binary_from_probe()") != NULL);
    assert(strstr(file_content, "local needs_default_audio_info=\"${needs_default_audio_info_field#needs_default_audio_info=}\"") != NULL);
    assert(strstr(file_content, "head -n 2 \"$file\" 2>/dev/null | grep -qx '# PMI_AARCH64_SDL_COMPAT_WRAPPER=1'") != NULL);
    assert(strstr(file_content, "write_aarch64_sdl_compat_wrapper()") != NULL);
    assert(strstr(file_content, "# PMI_AARCH64_SDL_COMPAT_WRAPPER=1") != NULL);
    assert(strstr(file_content, "AARCH64_SDL_RUNTIME_LIB=\"$PAK_DIR/runtime/aarch64/lib\"") != NULL);
    assert(strstr(file_content, "export LD_LIBRARY_PATH=\"$AARCH64_SDL_RUNTIME_LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"") != NULL);
    assert(strstr(file_content, "process_aarch64_sdl_compat_for_port()") != NULL);
    assert(strstr(file_content, "refresh_aarch64_sdl_compat_wrappers()") != NULL);
    assert(strstr(file_content, "refresh_aarch64_sdl_compat_for_launcher()") != NULL);
    assert(strstr(file_content, "if ! port_dir=$(launcher_aarch64_port_dir \"$launcher_path\"); then") != NULL);
    assert(strstr(file_content, "refresh_port_probe_cache \"$port_dir\" || return 0") != NULL);
    assert(strstr(file_content, "cache_path=$(port_probe_cache_path \"$port_dir\")") != NULL);
    assert(strstr(file_content, "maybe_refresh_aarch64_sdl_compat_binary_from_probe \"$record_path\" \"$field1\"") != NULL);
    assert(strstr(file_content, "echo \"PMI_WARN aarch64_sdl_compat_missing=$runtime_sdl\"") != NULL);
    assert(strstr(file_content, "echo \"PMI_DIAG aarch64_sdl_compat_applied=$binary_path\"") != NULL);
    assert(strstr(file_content, "self_pid=\"$$\"") != NULL);
    assert(strstr(file_content, "pgrep -f \"$pattern\" | while IFS= read -r pid || [ -n \"$pid\" ]; do") != NULL);
    assert(strstr(file_content, "[ \"$pid\" = \"$self_pid\" ] && continue") != NULL);
    assert(strstr(file_content, "unzip -oq \"$PM_RUNTIME_ROOT/pylibs.zip\" -d \"$PM_RUNTIME_ROOT\"") != NULL);
    assert(strstr(file_content, "port_shell_from_json()") != NULL);
    assert(strstr(file_content, "sed -n 's|.*\"\\([^\"]*\\.sh\\)\".*|\\1|p' \"$port_json\" | head -n 1") != NULL);
    assert(strstr(file_content, "port_has_arch()") != NULL);
    assert(strstr(file_content, "printf '%s' \"$compact\" | grep -Eq") != NULL);
    assert(strstr(file_content, "rewrite_armhf_port_launcher()") != NULL);
    assert(strstr(file_content, "sed -i 's|cd \"$BASEDIR/64\"|cd \"$BASEDIR/32\"|g' \"$launcher_path\"") != NULL);
    assert(strstr(file_content, "sed -i 's|BOX64_LOG|BOX86_LOG|g' \"$launcher_path\"") != NULL);
    assert(strstr(file_content, "sed -i 's|BOX64_PATH|BOX86_PATH|g' \"$launcher_path\"") != NULL);
    assert(strstr(file_content, "sed -i 's|BOX64_LD_LIBRARY_PATH|BOX86_LD_LIBRARY_PATH|g' \"$launcher_path\"") != NULL);
    assert(strstr(file_content, "sed -i 's|^export BOX86_PATH=.*$|export BOX86_PATH=\"$BASEDIR/32/lib${BOX86_PATH:+:$BOX86_PATH}\"|' \"$launcher_path\"") != NULL);
    assert(strstr(file_content, "sed -i 's|\"$PAK_DIR/bin/box64\"|\"$GAMEDIR/box86/box86\"|g' \"$launcher_path\"") != NULL);
    assert(strstr(file_content, "sed -i 's|\"$PAK_DIR/bin/box86\"|\"$GAMEDIR/box86/box86\"|g' \"$launcher_path\"") != NULL);
    assert(strstr(file_content, "apply_port_arch_rewrites()") != NULL);
    assert(strstr(file_content, "if port_has_arch \"$port_json\" \"armhf\" || grep -q 'PORT_32BIT=Y' \"$launcher_path\" 2>/dev/null; then") != NULL);
    assert(strstr(file_content, "launcher_requires_armhf()") != NULL);
    assert(strstr(file_content, "bind_flip_libmali()") != NULL);
    assert(strstr(file_content, "launcher_requires_system_gl_stack()") != NULL);
    assert(strstr(file_content, "if [ \"$field1\" = \"has_bundled_native_gl_stack=1\" ]; then") != NULL);
    assert(strstr(file_content, "uses_sdl_gl_windowing=\"${field2#uses_sdl_gl_windowing=}\"") != NULL);
    assert(strstr(file_content, "is_wrapper=\"${field3#is_wrapper=}\"") != NULL);
    assert(strstr(file_content, "[ \"$uses_sdl_gl_windowing\" = \"1\" ] && [ \"$is_wrapper\" != \"1\" ]") != NULL);
    assert(strstr(file_content, "local source_lib=\"$PAK_DIR/files/libmali-g2p0.so.1.9.0\"") != NULL);
    assert(strstr(file_content, "echo \"PMI_WARN flip_libmali_missing=$source_lib\"") != NULL);
    assert(strstr(file_content, "echo \"PMI_DIAG flip_libmali_bound=/usr/lib/libmali.so.1.9.0\"") != NULL);
    assert(strstr(file_content, "write_artwork_png()") != NULL);
    assert(strstr(file_content, "\"$PAK_DIR/bin/pm-artwork-convert\" \"$artwork_file\" \"$dest_file\"") != NULL);
    assert(strstr(file_content, "port_json=\"$port_dir/port.json\"") != NULL);
    assert(strstr(file_content, "shell_script=$(port_shell_from_json \"$port_json\")") != NULL);
    assert(strstr(file_content, "write_artwork_png \"$artwork_file\" \"$dest_file\"") != NULL);
    assert(strstr(file_content, "copy_game_scripts") != NULL);
    assert(strstr(file_content, "write_game_wrapper()") != NULL);
    assert(strstr(file_content, "write_box64_wrapper()") != NULL);
    assert(strstr(file_content, "refresh_box_runtime_wrappers()") != NULL);
    assert(strstr(file_content, "find \"$search_path\" -type f \\( -path '*/box86/box86' -o -path '*/box64/box64' \\)") != NULL);
    assert(strstr(file_content, "write_box86_compat_wrapper \"$file\"") != NULL);
    assert(strstr(file_content, "write_box64_wrapper \"$file\"") != NULL);
    assert(strstr(file_content, "refresh_armhf_binary_wrappers()") != NULL);
    assert(strstr(file_content, "find \"$search_path\" -type f -name 'gmloader'") != NULL);
    assert(strstr(file_content, "write_armhf_exec_compat_wrapper \"$file\"") != NULL);
    assert(strstr(file_content, "process_aarch64_sdl_compat_for_port \"$port_dir\"") != NULL);
    assert(strstr(file_content, "seed_x86_runtime_libs()") != NULL);
    assert(strstr(file_content, "local runtime_dir=\"$PAK_DIR/lib/box64-i386-linux-gnu\"") != NULL);
    assert(strstr(file_content, "find \"$search_path\" -type d -path '*/gamedata/*/32/lib'") != NULL);
    assert(strstr(file_content, "[ -f \"$lib_dir/libstdc++.so.6\" ] || cp -f \"$runtime_dir/libstdc++.so.6\" \"$lib_dir/\"") != NULL);
    assert(strstr(file_content, "[ -f \"$lib_dir/libgcc_s.so.1\" ] || cp -f \"$runtime_dir/libgcc_s.so.1\" \"$lib_dir/\"") != NULL);
    assert(strstr(file_content, "local checker=\"$PAK_DIR/bin/pm-sdl-compat-check\"") == NULL);
    assert(strstr(file_content, "sdl_compat_check_cache_path()") == NULL);
    assert(strstr(file_content, "write_sdl_compat_check_cache()") == NULL);
    assert(strstr(file_content, "binary_uses_sdl_gl_windowing()") == NULL);
    assert(strstr(file_content, "port_has_bundled_native_gl_stack()") == NULL);
    assert(strstr(file_content, "is_elf64_file()") == NULL);
    assert(strstr(file_content, "is_probe_cache_artifact()") == NULL);
    assert(strstr(file_content, "is_aarch64_launch_probe_candidate()") == NULL);
    assert(strstr(file_content, "cleanup_port_probe_artifacts()") == NULL);
    assert(strstr(file_content, "list_port_aarch64_launch_probe_candidates()") == NULL);
    assert(strstr(file_content, "export PMI_PORT_SCRIPT=\"$target_script\"") != NULL);
    assert(strstr(file_content, "exec \"$PAK_DIR/launch.sh\" \"$wrapper_path\"") != NULL);
    assert(strstr(file_content, "process_squashfs_files") != NULL);
    assert(strstr(file_content, "sed -i \"s|/roms/ports/PortMaster|$PM_RUNTIME_ROOT|g\" \"$file\"") != NULL);
    assert(strstr(file_content, "sed -i \"s|/mnt/sdcard/Roms/.portmaster/PortMaster|$PM_RUNTIME_ROOT|g\" \"$file\"") != NULL);
    assert(strstr(file_content, "controlfolder=\"$EMU_DIR\"") == NULL);
    assert(strstr(file_content, "source $controlfolder/control.txt") == NULL);
    assert(strstr(file_content, "source $controlfolder/tasksetter") == NULL);
    assert(strstr(file_content, "systemctl >/dev/null 2>\\&1; then $ESUDO systemctl restart oga_events \\& fi") != NULL);
    assert(strstr(file_content, "local source_script=\"${PMI_PORT_SCRIPT:-$ROM_PATH}\"") != NULL);
    assert(strstr(file_content, "apply_port_arch_rewrites") != NULL);
    assert(strstr(file_content, "refresh_armhf_binary_wrappers \"$REAL_PORTS_DIR\"") != NULL);
    assert(strstr(file_content, "refresh_aarch64_sdl_compat_wrappers \"$REAL_PORTS_DIR\"") == NULL);
    assert(strstr(file_content, "refresh_aarch64_sdl_compat_for_launcher \"$source_script\"") != NULL);
    assert(strstr(file_content, "if launcher_requires_armhf \"$source_script\"; then") != NULL);
    assert(strstr(file_content, "seed_x86_runtime_libs \"$REAL_PORTS_DIR\"") != NULL);
    assert(strstr(file_content, "bind_flip_libmali") != NULL);
    assert(strstr(file_content, "start_power_lid_helper\n\n    while true; do") != NULL);
    assert(strstr(file_content, "stop_power_lid_helper\n    post_gui_rewrites") != NULL);
    assert(strstr(file_content, "if launcher_requires_system_gl_stack \"$source_script\"; then") != NULL);
    assert(strstr(file_content, "echo \"PMI_DIAG system_gl_stack_launcher=$source_script\"") != NULL);
    assert(strstr(file_content, "PMI_LD_LIBRARY_STRATEGY=system-gl bash \"$script_to_run\"") != NULL);
    assert(strstr(file_content, "start_power_lid_helper\n    if launcher_requires_system_gl_stack \"$source_script\"; then") != NULL);
    assert(strstr(file_content, "stop_power_lid_helper\n}") != NULL);
    assert(strstr(file_content, "stage_launch_script") != NULL);
    assert(strstr(file_content, "echo \"PMI_DIAG selected_port_script=$source_script\"") != NULL);
    assert(strstr(file_content, "echo \"PMI_DIAG rewritten_launch_path=$script_to_run\"") != NULL);
    assert(strstr(file_content, "if [ \"${PMI_TEST_MODE:-}\" = \"overlay-sync\" ]; then") != NULL);
    assert(strstr(file_content, "bash \"$script_to_run\"") != NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/harbourmaster/platform.py", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "Path(os.environ.get(\"XDG_DATA_HOME\", str(Path.home() / \".local/share\"))) / \"PortMaster\" / \"control.txt\"") != NULL);
    assert(strstr(file_content, "Path(os.environ.get(\"PMI_ROM_ROOT\", \"/mnt/SDCARD/Roms/PORTS\"))") != NULL);
    assert(strstr(file_content, "Path(\"/roms/ports/PortMaster/control.txt\")") == NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/miyoo/control.txt", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "controlfolder=\"") != NULL);
    assert(strstr(file_content, "/PortMaster") != NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/PortMaster.sh", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "Portmaster.sh not found relative to $SELF_DIR") != NULL);
    assert(strstr(file_content, "exec \"$PAK_DIR/Portmaster.sh\" \"$@\"") != NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/harbourmaster/hardware.py", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "bundled_armhf = Path(os.environ.get('HM_TOOLS_DIR', '')) / 'runtime' / 'armhf'") != NULL);
    assert(strstr(file_content, "bundled_armhf.joinpath('lib', 'ld-linux-armhf.so.3').exists()") != NULL);
    assert(strstr(file_content, "bundled_armhf.joinpath('miyoo355_rootfs_32.img_partaa').exists()") != NULL);
    assert(strstr(file_content, "if 'armhf' not in info[\"capabilities\"]:") != NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pugwash", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "def portmaster_check_update(pm, config, temp_dir):\n    return False\n") != NULL);
    assert(strstr(file_content, "def start_artwork_warmup(self):") != NULL);
    assert(strstr(file_content, "self.images.warmup_jpeg_directory(images_dir)") != NULL);
    assert(strstr(file_content, "def poll_artwork_cache_updates(self):") != NULL);
    assert(strstr(file_content, "completed = self.images.poll_completed_jpegs()") != NULL);
    assert(strstr(file_content, "pm.start_artwork_warmup()") != NULL);
    assert(strstr(file_content, "self.poll_artwork_cache_updates()") != NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/pySDL2gui.py", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "def resolve_image_path(self, res_filename):") != NULL);
    assert(strstr(file_content, "self.jpeg_cache_root = Path(os.environ.get(\"XDG_CACHE_HOME\", str(Path.home() / \".cache\"))) / \"PortMaster\" / \"jpeg-cache\"") != NULL);
    assert(strstr(file_content, "self.jpeg_high_priority = queue.Queue()") != NULL);
    assert(strstr(file_content, "def _queue_jpeg_conversion(self, source_path, *, high_priority):") != NULL);
    assert(strstr(file_content, "def warmup_jpeg_directory(self, directory):") != NULL);
    assert(strstr(file_content, "def poll_completed_jpegs(self):") != NULL);
    assert(strstr(file_content, "self._queue_jpeg_conversion(source_path, high_priority=True)") != NULL);
    assert(strstr(file_content, "return str(cached_path)") != NULL);
    assert(strstr(file_content, "res_filename = self.resolve_image_path(res_filename)") != NULL);
    assert(strstr(file_content, "return self.images.get(\"NO_IMAGE\", None)") != NULL);
    assert(strstr(file_content, "except Exception:\n                if filename != \"NO_IMAGE\":") != NULL);
    assert(strstr(file_content, "except Exception:\n            stored_name = data.get(\"name\", file_name)") != NULL);
    assert(strstr(file_content, "cached_name = f\"{res_path}.pm.png\"") == NULL);
    assert(strstr(file_content, "subprocess.run([converter, res_path, cached_name], check=True,") == NULL);
    free(file_content);
    file_content = NULL;

    snprintf(file_path, sizeof(file_path), "%s/PortMaster/pylibs/default_theme/theme.json", payload_dir);
    assert(read_text_file_alloc(file_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "\"screenshot.png\": {\n                    \"name\": \"NO_IMAGE\"\n                }") != NULL);
    assert(strstr(file_content, "\"no-image.jpg\": {\n                    \"name\": \"NO_IMAGE\"\n                }") == NULL);
    free(file_content);
    file_content = NULL;

    assert(read_text_file_alloc(layout.manifest_path, &file_content, &file_len) == 0);
    assert(strstr(file_content, "\"installed_runtime_source\": \"portmaster_gui_trimui\"") != NULL);
    assert(strstr(file_content, "\"installed_runtime_version\": \"2026.04.01-1426\"") != NULL);
    assert(strstr(file_content, "\"installed_runtime_asset_name\": \"trimui.portmaster.zip\"") != NULL);
    free(file_content);
    file_content = NULL;

    assert(install_files_present(&layout) == 1);
    return 0;
}
