#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$project_root/build-package"
stage_dir="$project_root/.package-stage"
release_dir="$project_root/release"
package_name="deepin-dock-lyrics"
version=$(sed -n 's/^project(deepin_dock_lyrics VERSION \([^ ]*\).*/\1/p' "$project_root/CMakeLists.txt")
architecture=$(dpkg --print-architecture)

rm -rf "$build_dir" "$stage_dir"
cmake -S "$project_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DDEEPIN_DOCK_LYRICS_BUILD_TESTS=OFF
cmake --build "$build_dir" --parallel
DESTDIR="$stage_dir" cmake --install "$build_dir"

mkdir -p "$stage_dir/DEBIAN" "$release_dir"
sed -e "s/^Version: .*/Version: $version/" \
    -e "s/^Architecture: .*/Architecture: $architecture/" \
    "$project_root/debian/control" > "$stage_dir/DEBIAN/control"

dpkg-deb --root-owner-group --build "$stage_dir" \
    "$release_dir/${package_name}_${version}_${architecture}.deb"
