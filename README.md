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

## Run

Dry-run on stdout:

```bash
./build/sure-smartie-linux --config configs/stdout-example.json --once
```

Run against the physical display:

```bash
./build/sure-smartie-linux --config configs/sure-example.json
```

Run with the sample plugin:

```bash
./build/sure-smartie-linux --config configs/plugin-example.json --once
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
  "providers": ["cpu", "ram", "system", "network"],
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
