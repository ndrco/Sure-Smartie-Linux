#!/bin/sh

set -eu

ENV_FILE="/home/re/src/Sure-Smartie-linux/build/install-polkit-stage/etc/default/sure-smartie-linux"
BINARY="/home/re/src/Sure-Smartie-linux/build/install-polkit-stage/bin/sure-smartie-linux"

if [ -f "$ENV_FILE" ]; then
  # shellcheck disable=SC1090
  . "$ENV_FILE"
fi

if [ "${SURE_SMARTIE_CONFIG+x}" = "x" ]; then
  export SURE_SMARTIE_CONFIG
fi
if [ "${SURE_SMARTIE_LOG_LEVEL+x}" = "x" ]; then
  export SURE_SMARTIE_LOG_LEVEL
fi

active_service_name() {
  if ! command -v systemctl >/dev/null 2>&1; then
    return 1
  fi

  if [ "${SURE_SMARTIE_SERVICE_NAME+x}" = "x" ] && [ -n "${SURE_SMARTIE_SERVICE_NAME}" ]; then
    if systemctl is-active --quiet "$SURE_SMARTIE_SERVICE_NAME"; then
      printf '%s\n' "$SURE_SMARTIE_SERVICE_NAME"
      return 0
    fi
    return 1
  fi

  for service_name in sure-smartie-linux.service sure-smartie-linux-root.service; do
    if systemctl is-active --quiet "$service_name"; then
      printf '%s\n' "$service_name"
      return 0
    fi
  done

  return 1
}

freeze_service() {
  if service_name="$(active_service_name)"; then
    systemctl kill -s SIGSTOP "$service_name" || true
  fi
}

thaw_service() {
  if service_name="$(active_service_name)"; then
    systemctl kill -s SIGCONT "$service_name" || true
  fi
}

case "${1:-}" in
  pre)
    freeze_service
    "$BINARY" --backlight off || true
    ;;
  post)
    "$BINARY" --backlight on || true
    thaw_service
    ;;
esac

exit 0
