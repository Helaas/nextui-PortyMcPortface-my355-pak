# Porty McPortface

A PortMaster installer pak for [NextUI](https://github.com/LoveRetro/NextUI) that includes everything needed to install and update the upstream TrimUI PortMaster runtime directly on device.

## Description

Porty McPortface is a standalone tool pak for [NextUI](https://github.com/LoveRetro/NextUI), wrapping up [PortMaster](https://portmaster.games/), which organizes and simplifies the installation process of PC ports. It downloads the latest upstream `trimui.portmaster.zip` release, installs it into `Emus/<PLATFORM>/PORTS.pak`, applies the NextUI-specific launcher and compatibility files, and keeps that runtime updated without depending on device-side `jq`, `python3`, `git`, or a compiler toolchain.

## Features

- Install and update PortMaster from a single NextUI tool pak.
- Distributed as a single `.pakz`, with no additional software required on the device.
- Rewrites the installed runtime for NextUI launchers and artwork support.
- Bundles `box64` support and downloads the Spruce ARMHF rootfs needed by supported 32-bit x86 ports.

## Supported Platforms

Porty McPortface is designed and tested for the following platform:

- `my355`

> [!IMPORTANT]
> Porty McPortface is intended for NextUI devices using the `my355` platform only.

## Disclaimer

This project is not officially supported by PortMaster. Please do **not** report issues related to Porty McPortface to the official PortMaster project or its maintainers.

Porty McPortface will never have 100% compatibility with all ports. If a port does not run correctly here, treat that as a limitation of this unofficial integration unless proven otherwise.

## Installation

1. Mount your NextUI SD card to your computer.
2. Download the latest [release](https://github.com/kevinvranken/nextui-portmaster-pak/releases) from GitHub. The release file is named `Porty-McPortface.pakz`.
3. Copy the `.pakz` file to the root of your SD card.
4. Eject your SD card and insert it back into your device.
5. Restart your device. NextUI will automatically detect and install the new pak.
6. Open **Tools** and launch **Porty McPortface**.
7. Read and accept the unsupported-integration warning.
8. Press `A` to install the selected PortMaster runtime.

## Usage

- From NextUI, launch **Porty McPortface** from **Tools** whenever you want to install, update, or reinstall the managed PortMaster runtime.
- After installation, go to **Ports** and launch **Portmaster** to browse and install ports.
- Installed ports will appear under the **Ports** section in NextUI.

> [!TIP]
> Not all ports are ready to run immediately after installation, and some still require files from a purchased copy of the game. Please refer to the port documentation on the [PortMaster](https://portmaster.games/games.html) website for setup instructions.

## Updating

The steps below update PortMaster while preserving your installed data and settings:

1. Launch **Porty McPortface** from **Tools**.
2. If a newer upstream TrimUI runtime is available, it will be shown on the main screen.
3. Press `A` to update to the selected version, or `Y` to choose an older supported release first.
4. If you need to refresh the managed runtime without changing versions, press `X` to reinstall it.

## Artwork

Artwork for installed ports is displayed in NextUI. Porty McPortface converts PortMaster JPEG artwork into cached PNG files that NextUI can load through the firmware image stack.

## Known Issues

- First installs and some runtime repairs can take a while because files need to be downloaded, unpacked, and patched.
- Some ports still require manual game data, extra fixes, or remain incompatible with this unofficial integration.

## Troubleshooting

- Runtime logs are written to `PORTS.txt` through the NextUI userdata log path when available.
- If the installer cannot save the warning acknowledgement, check that the SD card is writable and that userdata directories can be created.
- If a port stops working after a reinstall, launch PortMaster and reinstall that specific port from inside PortMaster.

## Thanks

- The PortMaster team for all their hard work.
- ro8inmorgan, frysee and the rest of the NextUI contributors for developing NextUI.
- And Spruce for providing the bins

## License

PortMaster is open-source software licensed under the MIT License. See the upstream PortMaster license for details.

The libraries and binaries contained in the bundled runtime directories are third-party components. They are licensed under their respective licenses and are not part of this project.

The Porty McPortface project code in this repository is licensed under the MIT License.
