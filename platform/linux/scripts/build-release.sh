#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$script_dir/../../.." && pwd)
build_dir=${1:-"$repository_dir/build/linux-release"}
package_dir=${2:-"$repository_dir/dist/linux"}

cd "$repository_dir"
cargo build -p vigo-core --release
cmake -S platform/linux -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVIGO_CORE_LIBRARY="$repository_dir/target/release/libvigo_core.a" \
    -DVIGO_BUILD_IBUS=ON \
    -DVIGO_BUILD_FCITX5=ON \
    -DBUILD_TESTING=ON
cmake --build "$build_dir"
ctest --test-dir "$build_dir" --output-on-failure
if command -v dpkg-deb >/dev/null 2>&1; then
    cpack --config "$build_dir/CPackConfig.cmake" -G DEB -B "$package_dir"
fi
if command -v rpmbuild >/dev/null 2>&1; then
    cpack --config "$build_dir/CPackConfig.cmake" -G RPM -B "$package_dir"
fi
