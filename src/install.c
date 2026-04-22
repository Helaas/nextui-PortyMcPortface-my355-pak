#include "install.h"
#include "fs.h"
#include "http.h"
#include "json.h"
#include "process.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SPRUCE_FLIP_RAW_BASE "https://raw.githubusercontent.com/spruceUI/spruceOS/main/spruce/flip"

static int copy_payload_compatibility_files(const install_layout *layout);
static int install_runtime_support_tools(const install_layout *layout);
static int unpack_runtime_pylibs(const install_layout *layout);
static int patch_runtime_compatibility_files(const install_layout *layout);
static int materialize_overlay_files(const install_layout *layout);
static int write_rom_stub(const install_layout *layout);
static int resolve_extractor_command(char *buffer, size_t buffer_size, const char *tool_pak_dir);
static int ensure_file_parent_dir(const char *path);
static int download_if_missing(const char *url, const char *output_path);
static int install_armhf_runtime(const install_layout *layout);

static const char *ASYNC_JPEG_IMAGE_MANAGER_PATCH =
    "        import queue\n"
    "        import threading\n"
    "\n"
    "        self.cache = []\n"
    "        self.jpeg_cache_root = Path(os.environ.get(\"XDG_CACHE_HOME\", str(Path.home() / \".cache\"))) / \"PortMaster\" / \"jpeg-cache\"\n"
    "        self.jpeg_cache_enabled = False\n"
    "        try:\n"
    "            self.jpeg_cache_root.mkdir(parents=True, exist_ok=True)\n"
    "            self.jpeg_cache_enabled = True\n"
    "        except OSError:\n"
    "            pass\n"
    "        self.jpeg_converter = os.environ.get(\"PMI_ARTWORK_CONVERTER\", \"pm-artwork-convert\")\n"
    "        self.jpeg_high_priority = queue.Queue()\n"
    "        self.jpeg_low_priority = queue.Queue()\n"
    "        self.jpeg_completed = queue.Queue()\n"
    "        self.jpeg_lock = threading.Lock()\n"
    "        self.jpeg_pending = {}\n"
    "        self.jpeg_failed = {}\n"
    "        self.jpeg_promoted = set()\n"
    "        self.jpeg_warmup_started = set()\n"
    "        self.jpeg_workers = []\n"
    "        if self.jpeg_cache_enabled:\n"
    "            for worker_index in range(2):\n"
    "                worker = threading.Thread(target=self._jpeg_worker, name=f\"pm-jpeg-{worker_index}\", daemon=True)\n"
    "                worker.start()\n"
    "                self.jpeg_workers.append(worker)\n"
    "\n"
    "    def _jpeg_cache_entry(self, res_filename):\n"
    "        import hashlib\n"
    "\n"
    "        source_path = Path(os.path.abspath(os.fspath(res_filename)))\n"
    "        digest = hashlib.sha1(str(source_path).encode(\"utf-8\")).hexdigest()\n"
    "        return source_path, self.jpeg_cache_root / f\"{digest}.png\"\n"
    "\n"
    "    def _jpeg_cache_ready(self, source_path, cached_path, source_mtime=None):\n"
    "        try:\n"
    "            if source_mtime is None:\n"
    "                source_mtime = source_path.stat().st_mtime\n"
    "            return cached_path.is_file() and cached_path.stat().st_mtime >= source_mtime\n"
    "        except OSError:\n"
    "            return False\n"
    "\n"
    "    def _queue_jpeg_conversion(self, source_path, *, high_priority):\n"
    "        if not self.jpeg_cache_enabled:\n"
    "            return None\n"
    "\n"
    "        try:\n"
    "            source_path, cached_path = self._jpeg_cache_entry(source_path)\n"
    "            source_mtime = source_path.stat().st_mtime\n"
    "        except OSError:\n"
    "            return None\n"
    "\n"
    "        if self._jpeg_cache_ready(source_path, cached_path, source_mtime):\n"
    "            return str(cached_path)\n"
    "\n"
    "        source_key = str(source_path)\n"
    "        queue_item = (source_key, str(cached_path), source_mtime)\n"
    "        target_queue = self.jpeg_high_priority if high_priority else self.jpeg_low_priority\n"
    "\n"
    "        with self.jpeg_lock:\n"
    "            if self.jpeg_pending.get(source_key) == source_mtime:\n"
    "                if not high_priority or source_key in self.jpeg_promoted:\n"
    "                    return None\n"
    "                self.jpeg_promoted.add(source_key)\n"
    "            else:\n"
    "                if self.jpeg_failed.get(source_key) == source_mtime:\n"
    "                    return None\n"
    "                self.jpeg_pending[source_key] = source_mtime\n"
    "                if not high_priority:\n"
    "                    self.jpeg_promoted.discard(source_key)\n"
    "\n"
    "        target_queue.put(queue_item)\n"
    "        return None\n"
    "\n"
    "    def _jpeg_worker(self):\n"
    "        import queue\n"
    "        import subprocess\n"
    "\n"
    "        while True:\n"
    "            try:\n"
    "                job = self.jpeg_high_priority.get_nowait()\n"
    "            except queue.Empty:\n"
    "                try:\n"
    "                    job = self.jpeg_low_priority.get(timeout=0.5)\n"
    "                except queue.Empty:\n"
    "                    continue\n"
    "\n"
    "            source_key, cached_name, source_mtime = job\n"
    "            source_path = Path(source_key)\n"
    "            cached_path = Path(cached_name)\n"
    "\n"
    "            if self._jpeg_cache_ready(source_path, cached_path, source_mtime):\n"
    "                with self.jpeg_lock:\n"
    "                    if self.jpeg_pending.get(source_key) == source_mtime:\n"
    "                        self.jpeg_pending.pop(source_key, None)\n"
    "                    self.jpeg_failed.pop(source_key, None)\n"
    "                    self.jpeg_promoted.discard(source_key)\n"
    "                self.jpeg_completed.put((source_key, cached_name))\n"
    "                continue\n"
    "\n"
    "            try:\n"
    "                cached_path.parent.mkdir(parents=True, exist_ok=True)\n"
    "                subprocess.run([self.jpeg_converter, source_key, cached_name], check=True,\n"
    "                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)\n"
    "            except Exception:\n"
    "                with self.jpeg_lock:\n"
    "                    if self.jpeg_pending.get(source_key) == source_mtime:\n"
    "                        self.jpeg_pending.pop(source_key, None)\n"
    "                    self.jpeg_failed[source_key] = source_mtime\n"
    "                    self.jpeg_promoted.discard(source_key)\n"
    "                continue\n"
    "\n"
    "            if self._jpeg_cache_ready(source_path, cached_path, source_mtime):\n"
    "                with self.jpeg_lock:\n"
    "                    if self.jpeg_pending.get(source_key) == source_mtime:\n"
    "                        self.jpeg_pending.pop(source_key, None)\n"
    "                    self.jpeg_failed.pop(source_key, None)\n"
    "                    self.jpeg_promoted.discard(source_key)\n"
    "                self.jpeg_completed.put((source_key, cached_name))\n"
    "            else:\n"
    "                with self.jpeg_lock:\n"
    "                    if self.jpeg_pending.get(source_key) == source_mtime:\n"
    "                        self.jpeg_pending.pop(source_key, None)\n"
    "                    self.jpeg_failed[source_key] = source_mtime\n"
    "                    self.jpeg_promoted.discard(source_key)\n"
    "\n"
    "    def warmup_jpeg_directory(self, directory):\n"
    "        import threading\n"
    "\n"
    "        if not self.jpeg_cache_enabled:\n"
    "            return\n"
    "\n"
    "        directory = Path(directory)\n"
    "        if not directory.is_dir():\n"
    "            return\n"
    "\n"
    "        directory_key = str(directory)\n"
    "        with self.jpeg_lock:\n"
    "            if directory_key in self.jpeg_warmup_started:\n"
    "                return\n"
    "            self.jpeg_warmup_started.add(directory_key)\n"
    "\n"
    "        threading.Thread(target=self._warmup_jpeg_directory, args=(directory,), daemon=True).start()\n"
    "\n"
    "    def _warmup_jpeg_directory(self, directory):\n"
    "        try:\n"
    "            entries = list(directory.iterdir())\n"
    "        except OSError:\n"
    "            return\n"
    "\n"
    "        for entry in entries:\n"
    "            if not entry.is_file():\n"
    "                continue\n"
    "            if entry.suffix.lower() not in (\".jpg\", \".jpeg\"):\n"
    "                continue\n"
    "            self._queue_jpeg_conversion(entry, high_priority=False)\n"
    "\n"
    "    def poll_completed_jpegs(self):\n"
    "        import queue\n"
    "\n"
    "        completed = []\n"
    "        while True:\n"
    "            try:\n"
    "                completed.append(self.jpeg_completed.get_nowait())\n"
    "            except queue.Empty:\n"
    "                return completed\n"
    "\n"
    "    def resolve_image_path(self, res_filename):\n"
    "        source_path = Path(os.fspath(res_filename))\n"
    "        if source_path.suffix.lower() not in (\".jpg\", \".jpeg\"):\n"
    "            return res_filename\n"
    "\n"
    "        source_path, cached_path = self._jpeg_cache_entry(source_path)\n"
    "        if self._jpeg_cache_ready(source_path, cached_path):\n"
    "            return str(cached_path)\n"
    "\n"
    "        self._queue_jpeg_conversion(source_path, high_priority=True)\n"
    "        return res_filename\n"
    "\n"
    "    def load(self, filename):\n";

static const char *PUGWASH_ARTWORK_WARMUP_PATCH =
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
    "    def start_artwork_warmup(self):\n"
    "        if self.hm is None:\n"
    "            return\n"
    "\n"
    "        images_dir = self.hm.cfg_dir / \"images_pm\"\n"
    "        self.images.warmup_jpeg_directory(images_dir)\n"
    "\n"
    "    def poll_artwork_cache_updates(self):\n"
    "        completed = self.images.poll_completed_jpegs()\n"
    "        if len(completed) == 0:\n"
    "            return\n"
    "\n"
    "        for source_path, cached_path in completed:\n"
    "            for key, value in list(self.text_data.items()):\n"
    "                if isinstance(value, str) and value == source_path:\n"
    "                    self.set_data(key, cached_path)\n"
    "\n"
    "    def set_port_info(self, port_name, port_info, want_install_size=False):\n";

int extract_stage_archive(const char *tool_pak_dir, const char *archive_path, const char *extract_dir) {
    char extractor[PATH_MAX];
    char quoted_extractor[PATH_MAX];
    char quoted_archive[PATH_MAX];
    char quoted_extract_dir[PATH_MAX];
    char command[PATH_MAX * 3];

    if (resolve_extractor_command(extractor, sizeof(extractor), tool_pak_dir) != 0)
        return -1;
    if (shell_quote(quoted_extractor, sizeof(quoted_extractor), extractor) != 0 ||
        shell_quote(quoted_archive, sizeof(quoted_archive), archive_path) != 0 ||
        shell_quote(quoted_extract_dir, sizeof(quoted_extract_dir), extract_dir) != 0)
        return -1;

    if (fs_ensure_dir(extract_dir) != 0)
        return -1;

    snprintf(command, sizeof(command), "%s x -y %s -o%s", quoted_extractor, quoted_archive, quoted_extract_dir);
    return run_command(command);
}

int load_install_state(const char *manifest_path, install_state *out) {
    char *manifest_json = NULL;
    size_t manifest_len = 0;
    int rc;

    memset(out, 0, sizeof(*out));
    if (!fs_path_exists(manifest_path))
        return -1;

    if (read_text_file_alloc(manifest_path, &manifest_json, &manifest_len) != 0)
        return -1;

    rc = parse_install_state(manifest_json, out);
    free(manifest_json);
    (void)manifest_len;
    return rc;
}

int install_files_present(const install_layout *layout) {
    char runtime_launch[PATH_MAX];
    char pugwash_path[PATH_MAX];
    char armhf_rootfs_partaa[PATH_MAX];

    snprintf(runtime_launch, sizeof(runtime_launch), "%s/launch.sh", layout->payload_pak_dir);
    snprintf(pugwash_path, sizeof(pugwash_path), "%s/PortMaster/pugwash", layout->payload_pak_dir);
    snprintf(armhf_rootfs_partaa, sizeof(armhf_rootfs_partaa), "%s/runtime/armhf/miyoo355_rootfs_32.img_partaa", layout->payload_pak_dir);

    return fs_path_exists(layout->manifest_path) &&
        fs_path_exists(layout->rom_stub_path) &&
        fs_path_exists(runtime_launch) &&
        fs_path_exists(pugwash_path) &&
        fs_path_exists(armhf_rootfs_partaa);
}

int install_from_stage(const char *stage_root, const install_layout *layout, const install_state *state) {
    char src[PATH_MAX];
    char dst[PATH_MAX];
    char manifest[4096];

    if (fs_ensure_dir(layout->payload_pak_dir) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/PortMaster", stage_root);
    snprintf(dst, sizeof(dst), "%s/PortMaster", layout->payload_pak_dir);
    if (replace_tree(src, dst) != 0)
        return -1;

    if (copy_payload_compatibility_files(layout) != 0)
        return -1;
    if (materialize_overlay_files(layout) != 0)
        return -1;
    if (install_runtime_support_tools(layout) != 0)
        return -1;
    if (unpack_runtime_pylibs(layout) != 0)
        return -1;
    if (patch_runtime_compatibility_files(layout) != 0)
        return -1;
    if (write_rom_stub(layout) != 0)
        return -1;
    if (layout->installed_tool_pak_dir != NULL && fs_remove_path(layout->installed_tool_pak_dir) != 0)
        return -1;

    if (format_install_state_json(state, manifest, sizeof(manifest)) != 0)
        return -1;
    if (write_text_file(layout->manifest_path, manifest) != 0)
        return -1;

    return 0;
}

static int copy_payload_compatibility_files(const install_layout *layout) {
    char src[PATH_MAX];
    char dst[PATH_MAX];

    snprintf(src, sizeof(src), "%s/launch.sh", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/launch.sh", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0 || chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/Portmaster.sh", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/Portmaster.sh", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0 || chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/control.txt", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/control.txt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(dst, sizeof(dst), "%s/PortMaster/control.txt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(dst, sizeof(dst), "%s/PortMaster/miyoo/control.txt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/PortMaster.txt", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/PortMaster.txt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(dst, sizeof(dst), "%s/PortMaster/PortMaster.txt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(dst, sizeof(dst), "%s/PortMaster/PortMaster.sh", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(dst, sizeof(dst), "%s/PortMaster/miyoo/PortMaster.txt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/config.py", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/config.py", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(dst, sizeof(dst), "%s/PortMaster/pylibs/harbourmaster/config.py", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/gamecontrollerdb.txt", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/gamecontrollerdb.txt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(dst, sizeof(dst), "%s/PortMaster/gamecontrollerdb.txt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/config.json", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/config.json", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/bin.tar.gz", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/bin.tar.gz", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/lib.tar.gz", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/lib.tar.gz", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/libmali-g2p0.so.1.9.0", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/libmali-g2p0.so.1.9.0", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/ca-certificates.crt", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/ca-certificates.crt", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/files/libffi.so.7", layout->payload_template_dir);
    snprintf(dst, sizeof(dst), "%s/files/libffi.so.7", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;

    return 0;
}

static int materialize_overlay_files(const install_layout *layout) {
    char runtime_dir[PATH_MAX];
    char temp_data_dir[PATH_MAX];
    char temp_data_dir_no_lead[PATH_MAX];
    char overlay_file[PATH_MAX];
    const char *files[] = {
        "%s/files/control.txt",
        "%s/PortMaster/control.txt",
        "%s/PortMaster/miyoo/control.txt"
    };
    size_t index;

    snprintf(runtime_dir, sizeof(runtime_dir), "%s/PortMaster", layout->payload_pak_dir);
    snprintf(temp_data_dir, sizeof(temp_data_dir), "%s/.ports_temp", layout->sdcard_root);
    if (temp_data_dir[0] == '/')
        snprintf(temp_data_dir_no_lead, sizeof(temp_data_dir_no_lead), "%s", temp_data_dir + 1);
    else
        snprintf(temp_data_dir_no_lead, sizeof(temp_data_dir_no_lead), "%s", temp_data_dir);

    for (index = 0; index < sizeof(files) / sizeof(files[0]); index++) {
        snprintf(overlay_file, sizeof(overlay_file), files[index], layout->payload_pak_dir);
        if (patch_text_file_all(overlay_file, "$PM_RUNTIME_ROOT", runtime_dir) != 0)
            return -1;
        if (patch_text_file_all(overlay_file, "$TEMP_DATA_DIR", temp_data_dir_no_lead) != 0)
            return -1;
    }

    return 0;
}

static int install_runtime_support_tools(const install_layout *layout) {
    char src[PATH_MAX];
    char dst[PATH_MAX];

    if (layout->runtime_tools_dir == NULL || !fs_path_exists(layout->runtime_tools_dir))
        return -1;

    snprintf(src, sizeof(src), "%s/pm-artwork-convert", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;

    snprintf(dst, sizeof(dst), "%s/bin/pm-artwork-convert", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/pm-sdl-compat-check", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;

    snprintf(dst, sizeof(dst), "%s/bin/pm-sdl-compat-check", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/pm-port-probe", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;

    snprintf(dst, sizeof(dst), "%s/bin/pm-port-probe", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/pm-power-lid-watch", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;

    snprintf(dst, sizeof(dst), "%s/bin/pm-power-lid-watch", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    if (install_armhf_runtime(layout) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/box64", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;

    snprintf(dst, sizeof(dst), "%s/bin/box64", layout->payload_pak_dir);
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/box64-i386-linux-gnu", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;
    snprintf(dst, sizeof(dst), "%s/lib/box64-i386-linux-gnu", layout->payload_pak_dir);
    if (replace_tree(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/box64-x86_64-linux-gnu", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;
    snprintf(dst, sizeof(dst), "%s/lib/box64-x86_64-linux-gnu", layout->payload_pak_dir);
    if (replace_tree(src, dst) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/aarch64/libSDL2-2.0.so.0", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;
    snprintf(dst, sizeof(dst), "%s/runtime/aarch64/lib/libSDL2-2.0.so.0", layout->payload_pak_dir);
    if (ensure_file_parent_dir(dst) != 0)
        return -1;
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/aarch64/pulse/libpulse-simple.so.0", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;
    snprintf(dst, sizeof(dst), "%s/runtime/aarch64/pulse/libpulse-simple.so.0", layout->payload_pak_dir);
    if (ensure_file_parent_dir(dst) != 0)
        return -1;
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/aarch64/pulse/libpulse.so.0", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;
    snprintf(dst, sizeof(dst), "%s/runtime/aarch64/pulse/libpulse.so.0", layout->payload_pak_dir);
    if (ensure_file_parent_dir(dst) != 0)
        return -1;
    if (copy_file(src, dst) != 0)
        return -1;
    if (chmod(dst, 0755) != 0)
        return -1;

    snprintf(src, sizeof(src), "%s/aarch64/pulse/libpulsecommon-13.99.so", layout->runtime_tools_dir);
    if (!fs_path_exists(src))
        return -1;
    snprintf(dst, sizeof(dst), "%s/runtime/aarch64/pulse/libpulsecommon-13.99.so", layout->payload_pak_dir);
    if (ensure_file_parent_dir(dst) != 0)
        return -1;
    if (copy_file(src, dst) != 0)
        return -1;
    return chmod(dst, 0755);
}

static int ensure_file_parent_dir(const char *path) {
    char parent[PATH_MAX];
    char *slash;

    if (snprintf(parent, sizeof(parent), "%s", path) >= (int)sizeof(parent))
        return -1;
    slash = strrchr(parent, '/');
    if (slash == NULL)
        return 0;
    *slash = '\0';
    return fs_ensure_dir(parent);
}

static int download_if_missing(const char *url, const char *output_path) {
    if (fs_path_exists(output_path))
        return 0;
    if (ensure_file_parent_dir(output_path) != 0)
        return -1;
    return http_download_file(url, output_path);
}

static int install_armhf_runtime(const install_layout *layout) {
    char src[PATH_MAX];
    char dst[PATH_MAX];
    char url[PATH_MAX];
    const char *parts[] = {
        "miyoo355_rootfs_32.img_partaa",
        "miyoo355_rootfs_32.img_partab",
        "miyoo355_rootfs_32.img_partac"
    };
    size_t index;

    snprintf(src, sizeof(src), "%s/armhf", layout->runtime_tools_dir);
    snprintf(dst, sizeof(dst), "%s/runtime/armhf", layout->payload_pak_dir);
    if (fs_path_exists(src))
        return replace_tree(src, dst);

    if (fs_ensure_dir(dst) != 0)
        return -1;

    for (index = 0; index < sizeof(parts) / sizeof(parts[0]); index++) {
        char output_path[PATH_MAX];

        snprintf(url, sizeof(url), "%s/%s", SPRUCE_FLIP_RAW_BASE, parts[index]);
        snprintf(output_path, sizeof(output_path), "%s/%s", dst, parts[index]);
        if (download_if_missing(url, output_path) != 0)
            return -1;
    }

    return 0;
}

static int unpack_runtime_pylibs(const install_layout *layout) {
    char runtime_dir[PATH_MAX];
    char pylibs_zip[PATH_MAX];
    char pylibs_md5[PATH_MAX];
    char quoted_runtime_dir[PATH_MAX];
    char command[PATH_MAX * 2];

    snprintf(runtime_dir, sizeof(runtime_dir), "%s/PortMaster", layout->payload_pak_dir);
    snprintf(pylibs_zip, sizeof(pylibs_zip), "%s/pylibs.zip", runtime_dir);
    snprintf(pylibs_md5, sizeof(pylibs_md5), "%s/pylibs.zip.md5", runtime_dir);

    if (!fs_path_exists(pylibs_zip))
        return 0;

    if (shell_quote(quoted_runtime_dir, sizeof(quoted_runtime_dir), runtime_dir) != 0)
        return -1;

    snprintf(command, sizeof(command),
        "cd %s && unzip -oq pylibs.zip && rm -f pylibs.zip pylibs.zip.md5",
        quoted_runtime_dir);
    if (run_command(command) != 0)
        return -1;

    if (fs_path_exists(pylibs_zip) || fs_path_exists(pylibs_md5))
        return -1;

    return 0;
}

static int patch_runtime_compatibility_files(const install_layout *layout) {
    char platform_py[PATH_MAX];
    char hardware_py[PATH_MAX];
    char pugwash[PATH_MAX];
    char pysdl2gui[PATH_MAX];
    char theme_json[PATH_MAX];
    const char *control_hack_expr =
        "Path(os.environ.get(\"XDG_DATA_HOME\", str(Path.home() / \".local/share\"))) / \"PortMaster\" / \"control.txt\"";

    snprintf(platform_py, sizeof(platform_py), "%s/PortMaster/pylibs/harbourmaster/platform.py", layout->payload_pak_dir);
    if (fs_path_exists(platform_py)) {
        if (patch_text_file_all(platform_py,
                "Path(\"/roms/ports/PortMaster/control.txt\")",
                control_hack_expr) != 0)
            return -1;
        if (patch_text_file_all(platform_py,
                "Path(\"/root/.local/share/PortMaster/control.txt\")",
                control_hack_expr) != 0)
            return -1;
        if (patch_text_file_all(platform_py,
                "\"/mnt/SDCARD/Roms/PORTS\"",
                "os.environ.get(\"PMI_ROM_ROOT\", \"/mnt/SDCARD/Roms/PORTS\")") != 0)
            return -1;
    }

    snprintf(hardware_py, sizeof(hardware_py), "%s/PortMaster/pylibs/harbourmaster/hardware.py", layout->payload_pak_dir);
    if (fs_path_exists(hardware_py)) {
        if (patch_text_file(hardware_py,
                "    if (\n"
                "            Path('/lib/ld-linux-x86-64.so.2').exists() or\n"
                "            Path('/lib64/ld-linux-x86-64.so.2').exists() or\n"
                "            Path('/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2').exists()):\n"
                "        info[\"capabilities\"].append(\"x86_64\")\n"
                "        info['primary_arch'] = \"x86_64\"\n"
                "\n"
                "    if HM_TESTING or 'primary_arch' not in info:\n",
                "    if (\n"
                "            Path('/lib/ld-linux-x86-64.so.2').exists() or\n"
                "            Path('/lib64/ld-linux-x86-64.so.2').exists() or\n"
                "            Path('/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2').exists()):\n"
                "        info[\"capabilities\"].append(\"x86_64\")\n"
                "        info['primary_arch'] = \"x86_64\"\n"
                "\n"
                "    bundled_armhf = Path(os.environ.get('HM_TOOLS_DIR', '')) / 'runtime' / 'armhf'\n"
                "    if (\n"
                "            bundled_armhf.joinpath('lib', 'ld-linux-armhf.so.3').exists() or\n"
                "            bundled_armhf.joinpath('miyoo355_rootfs_32.img').exists() or\n"
                "            bundled_armhf.joinpath('miyoo355_rootfs_32.img_partaa').exists()):\n"
                "        if 'armhf' not in info[\"capabilities\"]:\n"
                "            info[\"capabilities\"].append(\"armhf\")\n"
                "\n"
                "    if HM_TESTING or 'primary_arch' not in info:\n") != 0)
            return -1;
    }

    snprintf(pugwash, sizeof(pugwash), "%s/PortMaster/pugwash", layout->payload_pak_dir);
    if (fs_path_exists(pugwash)) {
        if (patch_text_file(pugwash,
                "def portmaster_check_update(pm, config, temp_dir):\n",
                "def portmaster_check_update(pm, config, temp_dir):\n    return False\n") != 0)
            return -1;
        if (patch_text_file(pugwash,
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
                "    def set_port_info(self, port_name, port_info, want_install_size=False):\n",
                PUGWASH_ARTWORK_WARMUP_PATCH) != 0)
            return -1;
        if (patch_text_file_all(pugwash,
                "            pm.hm = HarbourMaster(config, temp_dir=temp_dir, callback=pm)\n",
                "            pm.hm = HarbourMaster(config, temp_dir=temp_dir, callback=pm)\n"
                "            pm.start_artwork_warmup()\n") != 0)
            return -1;
        if (patch_text_file(pugwash,
                "        # Update scanning\n"
                "        if self.timers.elapsed('dir_scan_interval', 500, run_first=True):\n"
                "            self.dir_scanner.iterate(30)\n"
                "\n"
                "        ## Check for any keys changed in our template system.\n",
                "        # Update scanning\n"
                "        if self.timers.elapsed('dir_scan_interval', 500, run_first=True):\n"
                "            self.dir_scanner.iterate(30)\n"
                "\n"
                "        self.poll_artwork_cache_updates()\n"
                "\n"
                "        ## Check for any keys changed in our template system.\n") != 0)
            return -1;
    }

    snprintf(theme_json, sizeof(theme_json), "%s/PortMaster/pylibs/default_theme/theme.json", layout->payload_pak_dir);
    if (fs_path_exists(theme_json)) {
        if (patch_text_file_all(theme_json,
                "\"no-image.jpg\": {\n                    \"name\": \"NO_IMAGE\"\n                }",
                "\"screenshot.png\": {\n                    \"name\": \"NO_IMAGE\"\n                }") != 0)
            return -1;
    }

    snprintf(pysdl2gui, sizeof(pysdl2gui), "%s/PortMaster/pylibs/pySDL2gui.py", layout->payload_pak_dir);
    if (fs_path_exists(pysdl2gui)) {
        if (patch_text_file(pysdl2gui,
                "        self.cache = []\n"
                "\n"
                "    def load(self, filename):\n",
                ASYNC_JPEG_IMAGE_MANAGER_PATCH) != 0)
            return -1;

        if (patch_text_file(pysdl2gui,
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
                "            return self.images[filename]\n",
                "            res_filename = self.resolve_image_path(res_filename)\n"
                "\n"
                "            try:\n"
                "                surf = sdl2.ext.image.load_img(res_filename)\n"
                "            except Exception:\n"
                "                if filename != \"NO_IMAGE\":\n"
                "                    return self.images.get(\"NO_IMAGE\", None)\n"
                "                return None\n"
                "\n"
                "            texture = sdl2.ext.renderer.Texture(self.renderer, surf)\n"
                "\n"
                "            sdl2.SDL_FreeSurface(surf)\n"
                "\n"
                "            self.textures[filename] = texture\n"
                "            self.images[filename] = Image(texture, renderer=self.renderer)\n"
                "            self.cache.insert(0, filename)\n"
                "\n"
                "            return self.images[filename]\n") != 0)
            return -1;

        if (patch_text_file(pysdl2gui,
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
                "            surf = sdl2.ext.image.load_img(res_filename)\n",
                "        try:\n"
                "            if file_name.lower().endswith('.svg') and \"size\" in data:\n"
                "                image_size = data[\"size\"]\n"
                "\n"
                "                if len(image_size) != 2:\n"
                "                    return None\n"
                "\n"
                "                if not isinstance(image_size[0], int) or not isinstance(image_size[1], int):\n"
                "                    return None\n"
                "\n"
                "                surf = sdl2.ext.image.load_svg(res_filename, width=image_size[0], height=image_size[1])\n"
                "            else:\n"
                "                res_filename = self.resolve_image_path(res_filename)\n"
                "                surf = sdl2.ext.image.load_img(res_filename)\n"
                "        except Exception:\n"
                "            stored_name = data.get(\"name\", file_name)\n"
                "            if stored_name != \"NO_IMAGE\":\n"
                "                return self.images.get(\"NO_IMAGE\", None)\n"
                "            return None\n") != 0)
            return -1;
    }

    return 0;
}

static int write_rom_stub(const install_layout *layout) {
    char content[1024];
    int written = snprintf(content, sizeof(content),
        "#!/bin/sh\n"
        "set -eu\n"
        "ROM_DIR=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\n"
        "SDCARD_ROOT=$(CDPATH= cd -- \"$ROM_DIR/../..\" && pwd)\n"
        "exec \"$SDCARD_ROOT/Emus/%s/PORTS.pak/launch.sh\" \"$0\" \"$@\"\n",
        layout->platform_name);

    if (written < 0 || (size_t)written >= sizeof(content))
        return -1;
    if (write_text_file(layout->rom_stub_path, content) != 0)
        return -1;
    return chmod(layout->rom_stub_path, 0755);
}

static int resolve_extractor_command(char *buffer, size_t buffer_size, const char *tool_pak_dir) {
    char bundled[PATH_MAX];
    const char *env_override = getenv("PMI_7ZZS");
    const char *fallback_tools[] = { "7zzs", "7zz", "7z" };
    size_t i;

    if (env_override != NULL && env_override[0] != '\0') {
        if (snprintf(buffer, buffer_size, "%s", env_override) >= (int)buffer_size)
            return -1;
        return 0;
    }

    snprintf(bundled, sizeof(bundled), "%s/resources/bin/7zzs", tool_pak_dir);
    if (access(bundled, X_OK) == 0) {
        if (snprintf(buffer, buffer_size, "%s", bundled) >= (int)buffer_size)
            return -1;
        return 0;
    }

    for (i = 0; i < sizeof(fallback_tools) / sizeof(fallback_tools[0]); i++) {
        char command[64];

        snprintf(command, sizeof(command), "command -v %s >/dev/null 2>&1", fallback_tools[i]);
        if (run_command(command) == 0) {
            if (snprintf(buffer, buffer_size, "%s", fallback_tools[i]) >= (int)buffer_size)
                return -1;
            return 0;
        }
    }

    return -1;
}
