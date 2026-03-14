# Release Checklist

## Before tagging

- confirm `README.md` reflects the current install and run flow;
- choose and add a project license;
- update `CHANGELOG.md`;
- run local build and tests;
- verify the install script on a clean machine or VM if packaging changed;
- verify GUI launch and privileged save flow if GUI-related code changed.

## Validation commands

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/sure-smartie-linux --config configs/stdout-example.json --once
```

## Release assets

Build reproducible release archives for a tag:

```bash
./scripts/package-release.sh v0.1.0
```

This produces:

- `dist/Sure-Smartie-Linux-v0.1.0-linux-<arch>-portable.tar.gz`
- `dist/Sure-Smartie-Linux-v0.1.0-linux-<arch>-install-rootfs.tar.gz`
- `dist/Sure-Smartie-Linux-v0.1.0-SHA256SUMS.txt`

## Optional manual checks

- physical display output
- backlight on/off command
- systemd service start/stop behavior
- suspend/resume hook behavior
- plugin example loading
