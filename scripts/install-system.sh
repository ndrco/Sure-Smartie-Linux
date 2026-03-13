#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build}"
SERVICE_NAME="sure-smartie-linux-root.service"

usage() {
  cat <<'EOF'
Usage: ./scripts/install-system.sh [--user-service] [--root-service]

Default mode:
  --root-service    Install and enable sure-smartie-linux-root.service

Optional:
  --user-service    Install and enable sure-smartie-linux.service
  --help            Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --user-service)
      SERVICE_NAME="sure-smartie-linux.service"
      shift
      ;;
    --root-service)
      SERVICE_NAME="sure-smartie-linux-root.service"
      shift
      ;;
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
done

if [[ ${EUID} -eq 0 ]]; then
  SUDO=()
else
  SUDO=(sudo)
fi

jobs() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi
  getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1
}

run_optional_system_command() {
  local command_name="$1"
  shift

  if command -v "${command_name}" >/dev/null 2>&1; then
    "${SUDO[@]}" "${command_name}" "$@" >/dev/null 2>&1 || true
  fi
}

set_env_value() {
  local file_path="$1"
  local key="$2"
  local value="$3"
  local temp_file

  temp_file="$(mktemp)"
  if [[ -f "${file_path}" ]]; then
    grep -v "^${key}=" "${file_path}" > "${temp_file}" || true
  fi
  printf '%s=%s\n' "${key}" "${value}" >> "${temp_file}"
  "${SUDO[@]}" install -m 0644 "${temp_file}" "${file_path}"
  rm -f "${temp_file}"
}

echo "==> Configuring"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}"

echo "==> Building"
cmake --build "${BUILD_DIR}" -j"$(jobs)"

echo "==> Installing into /usr/local"
"${SUDO[@]}" cmake --install "${BUILD_DIR}"

echo "==> Installing helper files"
"${SUDO[@]}" install -D -m 0644 \
  "${BUILD_DIR}/install_manifest.txt" \
  "/usr/local/share/sure-smartie-linux/install_manifest.txt"
"${SUDO[@]}" install -D -m 0755 \
  "${REPO_ROOT}/scripts/uninstall-system.sh" \
  "/usr/local/bin/sure-smartie-uninstall"

echo "==> Refreshing application menu"
run_optional_system_command update-desktop-database /usr/local/share/applications
run_optional_system_command gtk-update-icon-cache -q -t /usr/local/share/icons/hicolor

echo "==> Reloading systemd"
"${SUDO[@]}" systemctl daemon-reload

if [[ "${SERVICE_NAME}" == "sure-smartie-linux.service" ]]; then
  SYSUSERS_FILE="/usr/local/lib/sysusers.d/sure-smartie-linux.conf"
  if [[ -f "${SYSUSERS_FILE}" ]]; then
    echo "==> Creating service user"
    "${SUDO[@]}" systemd-sysusers "${SYSUSERS_FILE}"
  fi
fi

echo "==> Configuring suspend/resume hook"
set_env_value "/usr/local/etc/default/sure-smartie-linux" \
              "SURE_SMARTIE_SERVICE_NAME" \
              "${SERVICE_NAME}"

echo "==> Restarting services"
"${SUDO[@]}" systemctl disable --now sure-smartie-linux.service sure-smartie-linux-root.service \
  >/dev/null 2>&1 || true
"${SUDO[@]}" systemctl enable --now "${SERVICE_NAME}"

cat <<EOF

Installation complete.

Enabled service:
  ${SERVICE_NAME}

Useful commands:
  systemctl status ${SERVICE_NAME}
  journalctl -u ${SERVICE_NAME} -f
  /usr/local/bin/sure-smartie-gui
  sudo sure-smartie-uninstall

Config files:
  /usr/local/etc/sure-smartie-linux/config.json
  /usr/local/etc/sure-smartie-linux/config.json.example

Desktop entry:
  /usr/local/share/applications/sure-smartie-gui.desktop

EOF
