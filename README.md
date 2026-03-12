# Sure-Smartie-linux

Linux-oriented C++20 utility for SURE Electronics 20x4 LCD Smartie Asset displays.

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

GUI build is enabled by default through `SURE_SMARTIE_BUILD_GUI=ON`.
If Qt6 Widgets is not available, CMake keeps building the CLI/core targets and
prints a status message that `sure-smartie-gui` was skipped.

To disable GUI explicitly:

```bash
cmake -S . -B build -DSURE_SMARTIE_BUILD_GUI=OFF
```

## Install

Staging install:

```bash
cmake --install build --prefix ./build/install
```

System-wide install:

```bash
sudo cmake --install build
```

By default this installs into `/usr/local`.

## Run

Dry-run on stdout:

```bash
./build/sure-smartie-linux --config configs/stdout-example.json --once
```

Run the GUI editor:

```bash
./build/sure-smartie-gui
```

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

The runtime first looks for `/usr/local/etc/sure-smartie-linux/config.json`.
If that file is absent, it automatically falls back to
`/usr/local/etc/sure-smartie-linux/config.json.example`, so a fresh install still
works before you create a custom config.

If the service should run without root, ensure the selected user has access to the
serial device, usually through the `dialout` group.

For suspend/resume handling, the installed sleep hook now:

- sends `--backlight off` before suspend
- sends `--backlight on` after resume
- temporarily pauses the running `sure-smartie-linux.service` with `SIGSTOP/SIGCONT`
  to avoid racing the display while the hook talks to the serial device

If you keep the runtime config in a non-default location, set `SURE_SMARTIE_CONFIG`
inside `/usr/local/etc/default/sure-smartie-linux` so both the service and the
sleep hook use the same file.

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

It also exposes a width estimator that is used by both CLI validation and the GUI line
counters, so template-heavy lines are no longer flagged just because the raw source
string is longer than 20 characters.

## GUI

The GUI edits the same JSON config consumed by `sure-smartie-linux`.

Current v1 capabilities:

- open, save and save-as for runtime-compatible JSON configs
- edit display settings, built-in providers, plugin paths and screen rotation data
- live preview rendered from the shared `TemplateEngine`
- live local metrics collected through the same built-in/plugin provider stack
- validation panel powered by the shared `ConfigValidator`

Current non-goals:

- direct serial writes from the GUI
- systemd management
- web UI

The preview widget renders the LCD frame directly and understands custom bar glyphs,
but it never opens the physical serial device.

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
