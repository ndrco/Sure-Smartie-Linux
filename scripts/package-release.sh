#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

version="${1:-v0.1.0}"
build_dir="${BUILD_DIR:-build-release}"
dist_dir="${DIST_DIR:-dist}"
arch="${ARCH:-$(uname -m)}"
build_type="${BUILD_TYPE:-Release}"
build_gui="${SURE_SMARTIE_BUILD_GUI:-ON}"
build_tests="${SURE_SMARTIE_BUILD_TESTS:-OFF}"

generator_args=()
if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
  generator_args=(-G "$CMAKE_GENERATOR")
elif command -v ninja >/dev/null 2>&1; then
  generator_args=(-G Ninja)
else
  generator_args=(-G "Unix Makefiles")
fi

asset_prefix="Sure-Smartie-Linux-${version}-linux-${arch}"
portable_root="${dist_dir}/${asset_prefix}-portable"
stage_root="${dist_dir}/${asset_prefix}-stage"
portable_tree="${portable_root}/portable"
checksums_file="${dist_dir}/Sure-Smartie-Linux-${version}-SHA256SUMS.txt"
portable_tar="${dist_dir}/${asset_prefix}-portable.tar.gz"
install_tar="${dist_dir}/${asset_prefix}-install-rootfs.tar.gz"

mkdir -p "$dist_dir"

cmake -S . -B "$build_dir" \
  "${generator_args[@]}" \
  -DCMAKE_BUILD_TYPE="$build_type" \
  -DSURE_SMARTIE_BUILD_GUI="$build_gui" \
  -DSURE_SMARTIE_BUILD_TESTS="$build_tests"

cmake --build "$build_dir" -j"$(nproc)"

rm -rf "$portable_root" "$stage_root"
install -d "${portable_tree}"

env DESTDIR="$stage_root" cmake --install "$build_dir" --prefix /usr/local

cp -a "${stage_root}/usr" "${portable_tree}/usr"
install -m 755 scripts/install-prebuilt-release.sh "${portable_tree}/install-release.sh"
install -m 755 scripts/uninstall-system.sh "${portable_tree}/uninstall-release.sh"

for file in README.md LICENSE Docs/releases/"${version}".md; do
  if [[ -f "$file" ]]; then
    install -m 644 "$file" "${portable_tree}/"
  fi
done

if command -v strip >/dev/null 2>&1; then
  find "${portable_tree}/usr" "$stage_root" -type f \( -perm -111 -o -name "*.so" \) \
    -exec strip --strip-unneeded {} + 2>/dev/null || true
fi

tar -C "$portable_root" -czf "$portable_tar" portable
tar -C "$stage_root" -czf "$install_tar" usr

sha256sum "$install_tar" "$portable_tar" > "$checksums_file"

printf 'Created:\n'
printf '  %s\n' "$portable_tar"
printf '  %s\n' "$install_tar"
printf '  %s\n' "$checksums_file"
