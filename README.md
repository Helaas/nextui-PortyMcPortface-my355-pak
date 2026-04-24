# Porty McPortface - Miyoo Flip (my355)
A PortMaster installer pak for [NextUI](https://github.com/LoveRetro/NextUI) that includes everything needed to install and update the upstream TrimUI PortMaster GUI runtime directly on device.

## Description

Porty McPortface is a standalone tool pak for [NextUI](https://github.com/LoveRetro/NextUI), wrapping up [PortMaster](https://portmaster.games/), which organizes and simplifies the installation process of PC ports. It downloads the latest upstream `trimui.portmaster.zip` release, installs it into `Emus/<PLATFORM>/PORTS.pak`, then retargets that runtime for `my355` with local launchers, compatibility files, and runtime support assets. It keeps that managed runtime updated without depending on device-side `jq`, `python3`, `git`, or a compiler toolchain.

## Features

- Install and update PortMaster from a single NextUI tool pak.
- Distributed as a single `.pakz`, with no additional software required on the device.
- Rewrites the installed runtime for NextUI launchers and artwork support.
- Adds NextUI-style power button and lid-close suspend handling for `my355` ports and the PortMaster GUI.
- Bundles `box64` support and downloads the Spruce ARMHF rootfs needed by supported 32-bit x86 ports.

## Supported Platforms

Porty McPortface is designed and tested for the following platform:

- `my355`

> [!IMPORTANT]
> Porty McPortface is intended for NextUI devices using the `my355` platform only.

## Upstream Integration

Earlier iterations of this project also targeted `tg5050`, which is why the upstream PortMaster base is still the TrimUI GUI runtime. That TrimUI path is no longer a supported target here; current releases intentionally target `my355` only.

We still use the upstream TrimUI asset because the installer retargets it locally during installation. After extracting `trimui.portmaster.zip`, the installer copies the local Miyoo-specific control overlay into both `PortMaster/control.txt` and `PortMaster/miyoo/control.txt`, and it also pulls the Spruce Flip ARMHF rootfs parts used by compatible ports. The upstream asset is TrimUI, but the installed integration is rewritten for `my355`.

This repository also carries adapted assets from [ben16w/minui-portmaster](https://github.com/ben16w/minui-portmaster): `launch.sh`, `files/control.txt`, `files/config.json`, `files/ca-certificates.crt`, `files/bin.tar.gz`, and the base `files/lib.tar.gz` bundle.

From [spruceUI/spruceOS](https://github.com/spruceUI/spruceOS) it uses `spruce/flip/miyoo355_rootfs_32.img_partaa`, `spruce/flip/miyoo355_rootfs_32.img_partab`, and `spruce/flip/miyoo355_rootfs_32.img_partac` at install time, plus the contents of `spruce/flip/lib/` when rebuilding the bundled runtime library archive. The bundled PortMaster controller database defaults to the Spruce X360 mapping, and the optional Nintendo layout uses Spruce's Nintendo mapping.

## Disclaimer

This project is not officially supported by PortMaster. Please do **not** report issues related to Porty McPortface to the official PortMaster project or its maintainers.

Porty McPortface will never have 100% compatibility with all ports. If a port does not run correctly here, treat that as a limitation of this unofficial integration unless proven otherwise.

## Installation

1. Mount your NextUI SD card to your computer.
2. Download the latest [release](https://github.com/Helaas/nextui-PortyMcPortface-my355-pak/releases) from GitHub. The release file is named `Porty-McPortface.pakz`.
3. Copy the `.pakz` file to the root of your SD card.
4. Eject your SD card and insert it back into your device.
5. Restart your device. NextUI will automatically detect and install the new pak.
6. Open **Tools** and launch **Porty McPortface**.
7. Read and accept the unsupported-integration warning.
8. Press `A RUN` to install the selected PortMaster runtime.

## Usage

- From NextUI, launch **Porty McPortface** from **Tools** whenever you want to install, update, or reinstall the managed PortMaster runtime.
- Press `Y SETTINGS` from Porty McPortface to choose the PortMaster runtime version and controller layout.
- After installation, go to **Ports** and launch **Portmaster** to browse and install ports.
- Installed ports will appear under the **Ports** section in NextUI.

## Porty Controls

Main screen:

- `A RUN` installs or updates the selected PortMaster runtime when work is available.
- `X REINSTALL` refreshes the managed runtime without changing the selected version.
- `Y SETTINGS` opens the on-device settings screen.
- `B QUIT` exits Porty McPortface. When the installed runtime is already current, there is no `A CLOSE`; use `B QUIT`.

Settings screen:

- `UP/DOWN` changes the focused setting row.
- `LEFT/RIGHT` changes the focused setting value.
- `PortMaster Version` cycles through available releases directly on the settings screen.
- `Global Controller Layout` switches between `X360` and `Nintendo`.
- `A OPEN` on **Port Layouts** opens the per-port layout screen.
- `START SAVE` commits staged settings on the current screen.
- `B BACK` exits Settings without saving staged changes.

> [!WARNING]
> The first time you launch **Portmaster** from the **Ports** section in NextUI, it can take up to a minute to start. Please wait for the initial setup to finish before assuming it has stalled.

> [!TIP]
> Not all ports are ready to run immediately after installation, and some still require files from a purchased copy of the game. Please refer to the port documentation on the [PortMaster](https://portmaster.games/games.html) website for setup instructions.

## Controller Layout

Porty McPortface defaults to the Spruce-style X360 button mapping for PortMaster ports.

- Open **Settings** from Porty McPortface and use **Global Controller Layout** to switch the default between `X360` and `Nintendo`.
- Use **Port Layouts** to set an installed port to `Follow Global`, `X360`, or `Nintendo`.
- The selected global or per-port layout is applied automatically the next time you launch a port.
- The PortMaster GUI itself always keeps the default X360 mapping; the Nintendo layout is only passed to launched ports.
- Advanced/manual global toggle: creating `/.userdata/my355/PORTS-portmaster/nintendo` enables Nintendo layout; deleting that file returns to X360.
- Advanced/manual per-port override: write `x360` or `nintendo` to `/.userdata/my355/PORTS-portmaster/controller-layouts/<launcher>.layout`, where `<launcher>` is the installed `.ports` launcher filename such as `AM2R.sh`. Delete the file to return that port to `Follow Global`.

## Updating

The steps below update PortMaster while preserving your installed data and settings:

1. Launch **Porty McPortface** from **Tools**.
2. If a newer upstream TrimUI runtime is available, it will be shown on the main screen.
3. Press `A RUN` to update to the selected version, or press `Y SETTINGS` and cycle the **PortMaster Version** row to an older supported release first.
4. If you need to refresh the managed runtime without changing versions, press `X` to reinstall it.

## Power & Lid

When running the managed `PORTS.pak` runtime on `my355`, Porty McPortface enables NextUI-style power handling for both the PortMaster GUI and launched ports:

- Short-press `POWER` to suspend.
- Close the lid to suspend.
- Hold `POWER` for about 1 second to shut down.

> [!WARNING]
> Shutdown does **not** create a save state or resume point. Any unsaved progress since the last in-game save will be lost.

## Artwork

Artwork for installed ports is displayed in NextUI. Porty McPortface converts PortMaster JPEG artwork into cached PNG files that NextUI can load through the firmware image stack.

## Known Issues

- First installs and some runtime repairs can take a while because files need to be downloaded, unpacked, and patched.
- Some ports still require manual game data, extra fixes, or remain incompatible with this unofficial integration.

## Troubleshooting

- Runtime logs are written to `PORTS.txt` through the NextUI userdata log path when available.
- If the installer cannot save the warning acknowledgement, check that the SD card is writable and that userdata directories can be created.
- If a port stops working after a reinstall, launch PortMaster and reinstall that specific port from inside PortMaster.

## Acknowledgements

Special thanks to [ben16w](https://github.com/ben16w) and [minui-portmaster](https://github.com/ben16w/minui-portmaster) for the original TrimUI and NextUI PortMaster packaging this project builds on, and to [spruceUI](https://github.com/spruceUI) and [spruceOS](https://github.com/spruceUI/spruceOS) for the Miyoo Flip runtime pieces used here.

Additional thanks to the [PortMaster team](https://portmaster.games/), [ro8inmorgan](https://github.com/ro8inmorgan), [frysee](https://github.com/frysee), and the rest of the [NextUI contributors](https://github.com/LoveRetro/NextUI/graphs/contributors) for developing [NextUI](https://github.com/LoveRetro/NextUI).

## License

PortMaster is open-source software licensed under the MIT License. See the upstream PortMaster license for details.

The libraries and binaries contained in the bundled runtime directories are third-party components. They are licensed under their respective licenses and are not part of this project.

The Porty McPortface project code in this repository is licensed under the MIT License.
