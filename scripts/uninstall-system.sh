#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build}"
SYSTEM_MANIFEST="/usr/local/share/sure-smartie-linux/install_manifest.txt"
INSTALL_MANIFEST="${BUILD_DIR}/install_manifest.txt"

usage() {
  cat <<'EOF'
Usage: ./scripts/uninstall-system.sh

Stops both Sure Smartie systemd services, removes installed files using
build/install_manifest.txt, and cleans common runtime directories.
EOF
}

if [[ $# -gt 0 ]]; then
  case "$1" in
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
fi

if [[ -f "${SYSTEM_MANIFEST}" ]]; then
  INSTALL_MANIFEST="${SYSTEM_MANIFEST}"
elif [[ ! -f "${INSTALL_MANIFEST}" ]]; then
  echo "Install manifest not found." >&2
  echo "Checked:" >&2
  echo "  ${SYSTEM_MANIFEST}" >&2
  echo "  ${INSTALL_MANIFEST}" >&2
  exit 1
fi

if [[ ${EUID} -eq 0 ]]; then
  SUDO=()
else
  SUDO=(sudo)
fi

run_optional_system_command() {
  local command_name="$1"
  shift

  if command -v "${command_name}" >/dev/null 2>&1; then
    "${SUDO[@]}" "${command_name}" "$@" >/dev/null 2>&1 || true
  fi
}

echo "==> Stopping services"
"${SUDO[@]}" systemctl disable --now sure-smartie-linux.service sure-smartie-linux-root.service \
  >/dev/null 2>&1 || true

echo "==> Removing installed files"
mapfile -t installed_files < "${INSTALL_MANIFEST}"
for (( index=${#installed_files[@]}-1; index>=0; --index )); do
  path="${installed_files[index]}"
  [[ -n "${path}" ]] || continue
  "${SUDO[@]}" rm -f "${path}" 2>/dev/null || true
done

echo "==> Cleaning empty install directories"
printf '%s\n' "${installed_files[@]}" \
  | xargs -r -n1 dirname \
  | sort -r -u \
  | while read -r dir_path; do
      "${SUDO[@]}" rmdir --ignore-fail-on-non-empty "${dir_path}" 2>/dev/null || true
    done

echo "==> Cleaning runtime directories"
"${SUDO[@]}" rm -rf /var/lib/sure-smartie-linux /var/log/sure-smartie-linux

echo "==> Removing helper files"
"${SUDO[@]}" rm -f /usr/local/bin/sure-smartie-uninstall
"${SUDO[@]}" rm -f /usr/local/share/sure-smartie-linux/install_manifest.txt
"${SUDO[@]}" rmdir --ignore-fail-on-non-empty /usr/local/share/sure-smartie-linux 2>/dev/null || true

echo "==> Removing optional service account"
"${SUDO[@]}" userdel _sure-smartie 2>/dev/null || true
"${SUDO[@]}" groupdel _sure-smartie 2>/dev/null || true

echo "==> Reloading systemd"
"${SUDO[@]}" systemctl daemon-reload
"${SUDO[@]}" systemctl reset-failed sure-smartie-linux.service sure-smartie-linux-root.service \
  >/dev/null 2>&1 || true

echo "==> Refreshing application menu"
run_optional_system_command update-desktop-database /usr/local/share/applications
run_optional_system_command gtk-update-icon-cache -q -t /usr/local/share/icons/hicolor

echo
echo "Uninstall complete."
