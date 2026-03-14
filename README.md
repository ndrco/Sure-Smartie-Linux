# Sure-Smartie-linux

[![CI](https://github.com/ndrco/Sure-Smartie-Linux/actions/workflows/ci.yml/badge.svg)](https://github.com/ndrco/Sure-Smartie-Linux/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/ndrco/Sure-Smartie-Linux/blob/main/LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/ndrco/Sure-Smartie-Linux)](https://github.com/ndrco/Sure-Smartie-Linux/releases)
[![GitHub issues](https://img.shields.io/github/issues/ndrco/Sure-Smartie-Linux)](https://github.com/ndrco/Sure-Smartie-Linux/issues)

Linux-oriented C++20 utility for SURE Electronics 20x4 LCD Smartie Asset displays.

Repository: <https://github.com/ndrco/Sure-Smartie-Linux>

## Highlights

- native serial driver for SURE LCD Smartie Asset displays
- configurable CLI runtime and optional Qt6 GUI editor
- built-in CPU, GPU, RAM, system, and network providers
- install scripts, systemd units, sysusers, and Polkit integration
- plugin SDK and sample runtime-loadable provider plugin

## Maintainer

NDRco <ndrco@yahoo.com>

## Current state

This repository now contains the phase 1 baseline:

- CMake-based build for Ubuntu 24.04
- native SURE serial display driver
- `IDisplay` abstraction with `sure` and `stdout` implementations
- serial layer on top of POSIX `termios`
- built-in providers for CPU, GPU, RAM, system and network metrics
- template engine and screen rotation
- JSON configuration loading

Phase 2 extends that baseline with:

- built-in GPU provider
- pseudo-graphics bars via custom LCD characters
- dynamic `.so` providers through `dlopen`

Phase 3 adds deployment hardening:

- installable systemd service and sysusers fragments
- journald-friendly structured logging
- installable CMake-based plugin SDK for external providers

Phase 4 adds a native GUI editor:

- optional `Qt6 Widgets` application `sure-smartie-gui`
- live LCD preview based on the same config, providers and template engine
- config serialization and validation APIs shared with the CLI/runtime

## Repository layout

```text
Sure-Smartie-linux/
├── CMakeLists.txt
├── README.md
├── configs/
├── include/
├── plugins/
├── providers/
├── src/
│   └── gui/
├── tests/
├── Docs/
└── Tests/
```

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Dependencies for the full build on Ubuntu 24.04 are typically:

- CMake 3.24+
- a C++20 compiler
- POSIX serial support
- Qt6 Widgets for the GUI target

GUI build is enabled by default through `SURE_SMARTIE_BUILD_GUI=ON`.
If Qt6 Widgets is not available, CMake keeps building the CLI/core targets and
prints a status message that `sure-smartie-gui` was skipped.

To disable GUI explicitly:

```bash
cmake -S . -B build -DSURE_SMARTIE_BUILD_GUI=OFF
```

## Install

Recommended for most users:

```bash
./scripts/install-system.sh
```

This script:

- configures and builds the project;
- installs it into `/usr/local`;
- installs the GUI launcher into the desktop applications menu when Qt GUI is built;
- enables `sure-smartie-linux-root.service` by default;
- updates the shared env file for the suspend/resume hook.

To remove the program later:

```bash
sudo sure-smartie-uninstall
```

If you prefer the non-root service variant:

```bash
./scripts/install-system.sh --user-service
```

Staging install:

```bash
cmake --install build --prefix ./build/install
```

System-wide install:

```bash
sudo cmake --install build
```

By default this installs into `/usr/local`.

## Development

Headless and CI-friendly smoke testing:

```bash
ctest --test-dir build --output-on-failure
```

Staging install without touching the system:

```bash
cmake --install build --prefix ./build/install
```

Release archives for GitHub Releases:

```bash
./scripts/package-release.sh v0.1.0
```

This creates portable and staged install archives under `dist/` together with
`SHA256SUMS` for upload to a GitHub Release.

## Run

Dry-run on stdout:

```bash
./build/sure-smartie-linux --config configs/stdout-example.json --once
```

Run the GUI editor:

```bash
./build/sure-smartie-gui
```

By default the GUI first tries the same installed config path used by the
runtime/service:

```text
/usr/local/etc/sure-smartie-linux/config.json
```

If that file is missing, it falls back to
`/usr/local/etc/sure-smartie-linux/config.json.example`, and only then to
repo-local example configs for developer runs.

When the GUI is saving back into `/usr/local/etc/sure-smartie-linux/config.json`,
it first tries a normal user write and then falls back to a Polkit `pkexec`
prompt so you can authenticate from the desktop session without running the
whole GUI as root.

In that same system-config mode, `Apply` saves the config and restarts the
configured service, equivalent to `sudo systemctl restart sure-smartie-linux-root.service`
unless `/usr/local/etc/default/sure-smartie-linux` overrides the unit name via
`SURE_SMARTIE_SERVICE_NAME=...`.

Open a specific config in the GUI:

```bash
./build/sure-smartie-gui configs/sure-example.json
```

Run against the physical display:

```bash
./build/sure-smartie-linux --config configs/sure-example.json
```

Run with the sample plugin:

```bash
./build/sure-smartie-linux --config configs/plugin-example.json --once
```

Validate a config without starting the render loop:

```bash
./build/sure-smartie-linux --config configs/sure-example.json --validate-config
```

Apply a direct backlight command without starting the render loop:

```bash
./build/sure-smartie-linux --config configs/sure-example.json --backlight off
./build/sure-smartie-linux --config configs/sure-example.json --backlight on
```

Override the serial device at runtime:

```bash
./build/sure-smartie-linux --config configs/sure-example.json --device /dev/serial/by-id/your-display
```

Set log verbosity:

```bash
./build/sure-smartie-linux --config configs/sure-example.json --log-level debug
```

## systemd

The install step generates and installs `sure-smartie-linux.service`.
It also generates `sure-smartie-linux-root.service` for systems where Intel RAPL
`energy_uj` is readable only by root.
It also installs `sure-smartie-linux.conf` into `sysusers.d`, which creates the
`_sure-smartie` service user and adds it to `dialout`.
It also installs:

- `/usr/local/etc/default/sure-smartie-linux` as a shared environment file
- `/usr/lib/systemd/system-sleep/sure-smartie-linux` as a suspend/resume hook

Typical flow:

```bash
sudo cmake --install build
sudo systemctl daemon-reload
sudo systemd-sysusers /usr/local/lib/sysusers.d/sure-smartie-linux.conf
sudo systemctl enable --now sure-smartie-linux
```

For simpler day-to-day installation on a desktop machine, prefer:

```bash
./scripts/install-system.sh
```

and for removal:

```bash
sudo sure-smartie-uninstall
```

If `cpu.power_w` stays `--` because `/sys/devices/virtual/powercap/.../energy_uj`
is root-only on your distro, use the dedicated root unit instead of opening that
sysfs node to all users:

```bash
sudo cmake --install build
sudo systemctl daemon-reload
sudo systemctl disable --now sure-smartie-linux 2>/dev/null || true
sudo systemctl enable --now sure-smartie-linux-root
```

The root unit keeps the RAPL access scoped to the service itself. If you also use the
installed suspend/resume hook, you can optionally set this in
`/usr/local/etc/default/sure-smartie-linux`:

```bash
SURE_SMARTIE_SERVICE_NAME=sure-smartie-linux-root.service
```

The runtime first looks for `/usr/local/etc/sure-smartie-linux/config.json`.
If that file is absent, it automatically falls back to
`/usr/local/etc/sure-smartie-linux/config.json.example`, so a fresh install still
works before you create a custom config.

If the service should run without root, ensure the selected user has access to the
serial device, usually through the `dialout` group.

The GUI preview is a regular desktop app and usually runs as your normal user, so it may
still show `cpu.power_w = --` on Ubuntu/Debian systems that keep RAPL counters root-only.
That does not prevent the root systemd service from showing the correct value on the LCD.

On a normal service stop, `sure-smartie-linux` now clears the LCD and turns the
backlight off, so the display does not stay lit after shutdown.

For suspend/resume handling, the installed sleep hook now:

- sends `--backlight off` before suspend
- sends `--backlight on` after resume

## Contributing

Project contribution guidance lives in [CONTRIBUTING.md](CONTRIBUTING.md).

## Security

Please use the process in [SECURITY.md](SECURITY.md) for sensitive reports.

## Release notes

Project changes are tracked in [CHANGELOG.md](CHANGELOG.md). A lightweight release process
checklist is available in [Docs/release-checklist.md](Docs/release-checklist.md). Published
GitHub releases live at <https://github.com/ndrco/Sure-Smartie-Linux/releases>.

## License

This project is licensed under the [MIT License](LICENSE).
- temporarily pauses the running `sure-smartie-linux.service` with `SIGSTOP/SIGCONT`
  to avoid racing the display while the hook talks to the serial device

If you keep the runtime config in a non-default location, set `SURE_SMARTIE_CONFIG`
inside `/usr/local/etc/default/sure-smartie-linux` so both the service and the
sleep hook use the same file.

### Installed service config

After a system-wide install, the service uses:

- `/usr/local/etc/default/sure-smartie-linux` as the shared environment file
- `/usr/local/etc/sure-smartie-linux/config.json` as the main runtime config
- `/usr/local/etc/sure-smartie-linux/config.json.example` as the fallback example

Config lookup order:

1. `SURE_SMARTIE_CONFIG` from `/usr/local/etc/default/sure-smartie-linux`, if set
2. `/usr/local/etc/sure-smartie-linux/config.json`
3. `/usr/local/etc/sure-smartie-linux/config.json.example`

Typical edit flow:

```bash
sudoedit /usr/local/etc/sure-smartie-linux/config.json
sudo systemctl restart sure-smartie-linux-root.service
```

The service logs clean single-line entries that work well with journald, for example:

```text
level=info component=app msg="entering render loop" refresh_ms="1000" providers="5" screens="2"
```

## Config example

```json
{
  "device": "/dev/ttyUSB1",
  "baudrate": 9600,
  "refresh_ms": 1000,
  "display": {
    "type": "sure",
    "cols": 20,
    "rows": 4,
    "backlight": true,
    "contrast": 128,
    "brightness": 192
  },
  "cpu_fan": {
    "rpm_path": "",
    "max_rpm": 0
  },
  "providers": ["cpu", "gpu", "ram", "system", "network"],
  "plugin_paths": [],
  "screens": [
    {
      "name": "cpu_gpu",
      "interval_ms": 2000,
      "lines": [
        "CPU {bar:cpu.load,6} {cpu.load}%",
        "GPU {bar:gpu.load,6} {gpu.load}%",
        "VRM {gpu.mem_used}/{gpu.mem_total}",
        "{system.time} {system.hostname}"
      ]
    }
  ]
}
```

If your motherboard exposes the CPU fan through sysfs, set `cpu_fan.rpm_path` to that
exact file and `cpu_fan.max_rpm` to the known fan maximum. `cpu.fan_rpm` is read directly
from that path, and `cpu.fan_percent` is derived from `max_rpm`.

## Notes about the SURE protocol

The current driver uses the commands validated in `Tests/sure_lcd_test.py` and in the vendor PDF:

- handshake: `FE 53 75 72 65`
- write line: `FE 47 01 <row> + 20 chars`
- contrast: `FE 50 <value>`
- brightness: `FE 98 <value>`
- backlight on: `FE 42 00`
- backlight off: `FE 46`

The template engine also supports bar macros:

- `{bar:cpu.load,6}` renders a 6-cell bar for values in the default range `0..100`
- `{bar:gpu.mem_percent,8,100}` renders a bar with an explicit max value
- `{at:12}` pads the line so the next output starts at column 12 (1-based)
- `{glyph:name}` can also reference any user glyph declared in `custom_glyphs`

User glyphs are stored as 5x8 row bitmasks:

```json
"custom_glyphs": [
  {
    "name": "heart",
    "rows": [0, 10, 31, 31, 31, 14, 4, 0]
  }
]
```

It also exposes a width estimator that is used by both CLI validation and the GUI line
counters, so template-heavy lines are no longer flagged just because the raw source
string is longer than 20 characters.

## GUI

The GUI edits the same JSON config consumed by `sure-smartie-linux`.

Current v1 capabilities:

- open, save and save-as for runtime-compatible JSON configs
- edit display settings, built-in providers, plugin paths and screen rotation data
- configure an explicit CPU fan RPM sensor path and max RPM for `cpu.fan_*`
- edit 5x8 custom LCD glyphs and save them into the runtime JSON config
- live preview rendered from the shared `TemplateEngine`
- live local metrics collected through the same built-in/plugin provider stack
- validation panel powered by the shared `ConfigValidator`

Current non-goals:

- direct serial writes from the GUI
- systemd management
- web UI

The preview widget renders the LCD frame directly and understands both built-in and
user-defined glyph patterns, but it never opens the physical serial device.

Detailed Russian guide for GUI-based configuration, screen design, built-in providers
and plugin development:

- [Docs/sure-smartie-gui-guide.md](Docs/sure-smartie-gui-guide.md)

## Plugin SDK

After install, external plugins can use:

```cmake
find_package(SureSmartieLinux CONFIG REQUIRED)
target_link_libraries(your_plugin PRIVATE SureSmartieLinux::sure_smartie_plugin_sdk)
```

A complete example project is installed to:

```text
/usr/local/share/sure-smartie-linux/sdk/example-plugin
```
