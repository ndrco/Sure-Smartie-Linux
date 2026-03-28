# Changelog

All notable changes to this project should be documented in this file.

The format can stay lightweight. A simple list per release is enough.

## Unreleased

- Initial public repository preparation
- Native CLI runtime, GUI editor, install scripts, systemd packaging, and plugin SDK
- GitHub CI, issue templates, contribution docs, and release metadata
- Runtime and provider optimizations for lower refresh overhead

## v0.1.1

- Added `sure_smartie_disk_plugin` with metrics for root and `/mnt*` mounts from `fstab`
- Added missing-mount handling with `-` values and `disk.N.mounted`
- Added short disk labels: `disk.N.device_short` and `disk.N.mount_short`
- Added `fstab` cache by `mtime` and disk usage cache with 5-second TTL
- Improved `{at:column}` macro to support forced backward cursor positioning
- Improved template rendering fallback for `_short` disk metrics in GUI preview

## v0.1.2

- Fixed `sure_smartie_disk_plugin` handling for `autofs` placeholders (`device=systemd-1`, `fs=autofs`)
- Prevented expensive/blocking `statvfs()` calls on autofs placeholder entries
- Preferred real filesystem mount entries over autofs placeholders for the same mountpoint
