#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PAYLOAD_ROOT="${SCRIPT_DIR}/usr"
SERVICE_NAME="sure-smartie-linux-root.service"

usage() {
  cat <<'EOF'
Usage: ./install-release.sh [--user-service] [--root-service]

Installs the files bundled in this prebuilt release into /usr and /usr/local,
enables one systemd service variant, and installs the helper command:

  sudo sure-smartie-uninstall

Options:
  --root-service    Install and enable sure-smartie-linux-root.service (default)
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

if [[ ! -d "${PAYLOAD_ROOT}" ]]; then
  echo "Bundled release payload not found at: ${PAYLOAD_ROOT}" >&2
  exit 1
fi

if [[ ! -x "${PAYLOAD_ROOT}/local/bin/sure-smartie-linux" ]]; then
  echo "Bundled sure-smartie-linux binary is missing from the release payload." >&2
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
  "${SUDO[@]}" install -D -m 0644 "${temp_file}" "${file_path}"
  rm -f "${temp_file}"
}

manifest_temp="$(mktemp)"
trap 'rm -f "${manifest_temp}"' EXIT

echo "==> Installing bundled files"
while IFS= read -r -d '' dir_path; do
  rel_path="${dir_path#${PAYLOAD_ROOT}/}"
  [[ "${rel_path}" != "${dir_path}" ]] || continue
  "${SUDO[@]}" install -d "/usr/${rel_path}"
done < <(find "${PAYLOAD_ROOT}" -mindepth 1 -type d -print0 | sort -z)

while IFS= read -r -d '' file_path; do
  rel_path="${file_path#${PAYLOAD_ROOT}/}"
  target_path="/usr/${rel_path}"
  install_mode=0644
  if [[ -x "${file_path}" ]]; then
    install_mode=0755
  fi

  "${SUDO[@]}" install -D -m "${install_mode}" "${file_path}" "${target_path}"
  printf '%s\n' "${target_path}" >> "${manifest_temp}"
done < <(find "${PAYLOAD_ROOT}" -type f -print0 | sort -z)

echo "==> Installing uninstall helper"
"${SUDO[@]}" install -D -m 0755 \
  "${SCRIPT_DIR}/uninstall-release.sh" \
  "/usr/local/bin/sure-smartie-uninstall"
printf '%s\n' "/usr/local/bin/sure-smartie-uninstall" >> "${manifest_temp}"

echo "==> Writing install manifest"
"${SUDO[@]}" install -D -m 0644 \
  "${manifest_temp}" \
  "/usr/local/share/sure-smartie-linux/install_manifest.txt"

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

EOF
