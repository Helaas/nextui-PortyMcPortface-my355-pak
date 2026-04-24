SHELL := /bin/bash

APP_NAME := porty-mcportface
PAK_NAME := Porty McPortface
RELEASE_BASENAME := Porty-McPortface
APOSTROPHE_DIR := third_party/apostrophe
BUILD_DIR := build
DIST_DIR := $(BUILD_DIR)/release
STAGING_DIR := $(BUILD_DIR)/staging
SRC_FILES := $(shell find src -name '*.c' -print | sort)
RUNTIME_BIN_DIR := $(BUILD_DIR)/runtime-bin
RUNTIME_CACHE_DIR := $(BUILD_DIR)/cache/runtime-bin
RUNTIME_HELPERS_STAMP := $(RUNTIME_BIN_DIR)/.helpers.stamp
RUNTIME_BOX64_LIBS_STAMP := $(RUNTIME_BIN_DIR)/.box64-runtime-libs.stamp
RUNTIME_STAMP := $(RUNTIME_BIN_DIR)/.stamp

MY355_TOOLCHAIN := ghcr.io/loveretro/my355-toolchain:latest
COMMON_INCLUDES := -I$(APOSTROPHE_DIR)/include -I./src -I./third_party/jsmn

.PHONY: mac run-mac my355 package clean test-native refresh-portmaster-lib-bundle

mac:
	@mkdir -p $(BUILD_DIR)/mac
	cc -std=gnu11 -O0 -g \
		-DPLATFORM_MAC \
		$(COMMON_INCLUDES) \
		$(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image) \
		-o $(BUILD_DIR)/mac/$(APP_NAME) \
		$(SRC_FILES) \
		$(shell pkg-config --libs sdl2 SDL2_ttf SDL2_image) \
		-lm -lpthread

run-mac: mac
	./$(BUILD_DIR)/mac/$(APP_NAME)

my355:
	@mkdir -p $(BUILD_DIR)/my355
	docker run --rm \
		-v "$(CURDIR)":/workspace \
		$(MY355_TOOLCHAIN) \
		make -C /workspace -f ports/my355/Makefile BUILD_DIR=/workspace/$(BUILD_DIR)/my355

build/my355/7zzs:
	@mkdir -p build/my355
	sh scripts/build-7zz-from-source.sh $(CURDIR)/build/my355

$(RUNTIME_BIN_DIR):
	@mkdir -p "$@"

$(RUNTIME_BIN_DIR)/aarch64:
	@mkdir -p "$@"

$(RUNTIME_BIN_DIR)/aarch64/pulse:
	@mkdir -p "$@"

$(RUNTIME_CACHE_DIR):
	@mkdir -p "$@"

$(RUNTIME_HELPERS_STAMP): tools/pm-artwork-convert.c tools/pm-sdl-compat-check.c tools/pm-port-probe.c tools/pm-power-lid-watch.c tools/pm-armhf-exec-wrapper.c third_party/stb/stb_image.h third_party/stb/stb_image_write.h | $(RUNTIME_BIN_DIR)
	docker run --rm \
		-v "$(CURDIR)":/workspace \
		-w /workspace \
		$(MY355_TOOLCHAIN) \
		sh -lc 'aarch64-nextui-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -I/workspace/third_party/stb -o /workspace/build/runtime-bin/pm-artwork-convert /workspace/tools/pm-artwork-convert.c -lm && aarch64-nextui-linux-gnu-strip /workspace/build/runtime-bin/pm-artwork-convert && aarch64-nextui-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -o /workspace/build/runtime-bin/pm-sdl-compat-check /workspace/tools/pm-sdl-compat-check.c && aarch64-nextui-linux-gnu-strip /workspace/build/runtime-bin/pm-sdl-compat-check && aarch64-nextui-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -o /workspace/build/runtime-bin/pm-port-probe /workspace/tools/pm-port-probe.c && aarch64-nextui-linux-gnu-strip /workspace/build/runtime-bin/pm-port-probe && aarch64-nextui-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -o /workspace/build/runtime-bin/pm-power-lid-watch /workspace/tools/pm-power-lid-watch.c && aarch64-nextui-linux-gnu-strip /workspace/build/runtime-bin/pm-power-lid-watch && aarch64-nextui-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -o /workspace/build/runtime-bin/pm-armhf-exec-wrapper /workspace/tools/pm-armhf-exec-wrapper.c && aarch64-nextui-linux-gnu-strip /workspace/build/runtime-bin/pm-armhf-exec-wrapper'
	@touch $@

$(RUNTIME_BIN_DIR)/rsync: third_party/rsync/my355/rsync | $(RUNTIME_BIN_DIR)
	cp third_party/rsync/my355/rsync "$@"
	chmod +x "$@"

$(RUNTIME_BIN_DIR)/rsync.txt: third_party/rsync/my355/rsync.txt | $(RUNTIME_BIN_DIR)
	cp third_party/rsync/my355/rsync.txt "$@"

$(RUNTIME_BIN_DIR)/box64: scripts/build-box64-from-source.sh | $(RUNTIME_BIN_DIR) $(RUNTIME_CACHE_DIR)
	docker run --rm \
		-v "$(CURDIR)":/workspace \
		-w /workspace \
		$(MY355_TOOLCHAIN) \
		sh /workspace/scripts/build-box64-from-source.sh /workspace/build/runtime-bin /workspace/build/cache/runtime-bin/box64

$(RUNTIME_BOX64_LIBS_STAMP): scripts/fetch-box64-runtime-libs.sh | $(RUNTIME_BIN_DIR)
	sh scripts/fetch-box64-runtime-libs.sh "$(CURDIR)/build/runtime-bin"
	@touch $@

$(RUNTIME_BIN_DIR)/aarch64/libSDL2-2.0.so.0: third_party/sdl2/my355/libSDL2-2.0.so.0 | $(RUNTIME_BIN_DIR)/aarch64
	cp third_party/sdl2/my355/libSDL2-2.0.so.0 "$@"

$(RUNTIME_BIN_DIR)/aarch64/pulse/libpulse-simple.so.0: third_party/pulse/my355/libpulse-simple.so.0 | $(RUNTIME_BIN_DIR)/aarch64/pulse
	cp third_party/pulse/my355/libpulse-simple.so.0 "$@"

$(RUNTIME_BIN_DIR)/aarch64/pulse/libpulse.so.0: third_party/pulse/my355/libpulse.so.0 | $(RUNTIME_BIN_DIR)/aarch64/pulse
	cp third_party/pulse/my355/libpulse.so.0 "$@"

$(RUNTIME_BIN_DIR)/aarch64/pulse/libpulsecommon-13.99.so: third_party/pulse/my355/libpulsecommon-13.99.so | $(RUNTIME_BIN_DIR)/aarch64/pulse
	cp third_party/pulse/my355/libpulsecommon-13.99.so "$@"

$(RUNTIME_STAMP): $(RUNTIME_HELPERS_STAMP) $(RUNTIME_BIN_DIR)/rsync $(RUNTIME_BIN_DIR)/rsync.txt $(RUNTIME_BIN_DIR)/box64 $(RUNTIME_BOX64_LIBS_STAMP) $(RUNTIME_BIN_DIR)/aarch64/libSDL2-2.0.so.0 $(RUNTIME_BIN_DIR)/aarch64/pulse/libpulse-simple.so.0 $(RUNTIME_BIN_DIR)/aarch64/pulse/libpulse.so.0 $(RUNTIME_BIN_DIR)/aarch64/pulse/libpulsecommon-13.99.so
	@touch $@

build/my355/runtime-bin/.stamp: $(RUNTIME_STAMP)
	@rm -rf build/my355/runtime-bin
	@mkdir -p build/my355/runtime-bin
	cp -R build/runtime-bin/. build/my355/runtime-bin/
	@touch $@

test-native:
	@mkdir -p $(BUILD_DIR)/tests
	cc -std=c11 -Wall -Wextra tools/pm-sdl-compat-check.c -o $(BUILD_DIR)/tests/pm-sdl-compat-check
	sh tests/test_pm_sdl_compat_check.sh $(BUILD_DIR)/tests/pm-sdl-compat-check
	cc -std=c11 -Wall -Wextra tools/pm-port-probe.c -o $(BUILD_DIR)/tests/pm-port-probe
	sh tests/test_pm_port_probe.sh $(BUILD_DIR)/tests/pm-port-probe
	cc -std=c11 -Wall -Wextra -Itests/fixtures tests/test_pm_power_lid_watch.c -o $(BUILD_DIR)/tests/test_pm_power_lid_watch
	./$(BUILD_DIR)/tests/test_pm_power_lid_watch
	sh tests/test_build_box64_cache.sh
	cc -std=c11 -Wall -Wextra $(COMMON_INCLUDES) tests/test_platform.c src/platform.c -o $(BUILD_DIR)/tests/test_platform
	./$(BUILD_DIR)/tests/test_platform
	cc -std=c11 -Wall -Wextra $(COMMON_INCLUDES) tests/test_json.c src/json.c -o $(BUILD_DIR)/tests/test_json
	./$(BUILD_DIR)/tests/test_json
	cc -std=c11 -Wall -Wextra $(COMMON_INCLUDES) tests/test_status.c src/status.c -o $(BUILD_DIR)/tests/test_status
	./$(BUILD_DIR)/tests/test_status
	cc -std=c11 -Wall -Wextra $(COMMON_INCLUDES) tests/test_consent.c src/consent.c src/userdata.c src/fs.c -o $(BUILD_DIR)/tests/test_consent
	./$(BUILD_DIR)/tests/test_consent
	cc -std=c11 -Wall -Wextra $(COMMON_INCLUDES) tests/test_controller_layout.c src/controller_layout.c src/userdata.c src/fs.c -o $(BUILD_DIR)/tests/test_controller_layout
	./$(BUILD_DIR)/tests/test_controller_layout
	cc -std=c11 -Wall -Wextra $(COMMON_INCLUDES) tests/test_updater_flow.c src/updater_flow.c -o $(BUILD_DIR)/tests/test_updater_flow
	./$(BUILD_DIR)/tests/test_updater_flow
	cc -std=c11 -Wall -Wextra $(COMMON_INCLUDES) tests/test_install.c src/install.c src/fs.c src/http.c src/json.c src/process.c -o $(BUILD_DIR)/tests/test_install
	./$(BUILD_DIR)/tests/test_install
	cc -std=c11 -Wall -Wextra $(COMMON_INCLUDES) tests/test_payload_exists.c -o $(BUILD_DIR)/tests/test_payload_exists
	./$(BUILD_DIR)/tests/test_payload_exists
	sh tests/test_portmaster_overlay.sh
	sh tests/test_portmaster_bootstrap.sh
	sh tests/test_power_lid_helper.sh

refresh-portmaster-lib-bundle:
	sh scripts/rebuild-portmaster-lib-bundle.sh

package: my355 build/my355/7zzs build/my355/runtime-bin/.stamp
	@rm -rf $(STAGING_DIR)
	@mkdir -p "$(STAGING_DIR)/Tools/my355/$(PAK_NAME).pak/resources/bin"
	@mkdir -p "$(STAGING_DIR)/Tools/my355/$(PAK_NAME).pak/resources/runtime-bin/my355"
	cp "$(BUILD_DIR)/my355/$(APP_NAME)" "$(STAGING_DIR)/Tools/my355/$(PAK_NAME).pak/"
	cp launch.sh pak.json "$(STAGING_DIR)/Tools/my355/$(PAK_NAME).pak/"
	cp -R payload "$(STAGING_DIR)/Tools/my355/$(PAK_NAME).pak/"
	cp "$(BUILD_DIR)/my355/7zzs" "$(STAGING_DIR)/Tools/my355/$(PAK_NAME).pak/resources/bin/7zzs"
	cp -R "$(BUILD_DIR)/my355/runtime-bin/." "$(STAGING_DIR)/Tools/my355/$(PAK_NAME).pak/resources/runtime-bin/my355/"
	@mkdir -p $(DIST_DIR)/all
	cd $(STAGING_DIR) && zip -9 -r "$(CURDIR)/$(DIST_DIR)/all/$(RELEASE_BASENAME).pakz" . -x '.*'

clean:
	rm -rf $(BUILD_DIR)
