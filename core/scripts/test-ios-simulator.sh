#!/usr/bin/env bash
# Builds the Catch2 engine tests for the iOS Simulator (arm64, iOS 18.0) and
# runs them in a booted simulator with `simctl spawn`, which runs a
# command-line binary inside the simulator; it reads fixtures from host paths.
# Usage: test-ios-simulator.sh <skia-dir> <vcpkg-root> <simulator-udid>
set -euo pipefail

skia_dir=$1
vcpkg_root=$2
udid=$3
root=$(cd "$(dirname "$0")/../.." && pwd)
dir=$root/build/ios-simulator-tests

cmake -S "$root/core" -B "$dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=18.0 \
  -DCMAKE_TOOLCHAIN_FILE="$vcpkg_root/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-ios-simulator \
  -DVCPKG_MANIFEST_FEATURES=tests \
  -DINK_BUILD_TESTS=ON \
  -DSKIA_SOURCE_DIR="$skia_dir"
cmake --build "$dir" --target ink_tests

xcrun simctl bootstatus "$udid" -b
cd "$dir/tests"
xcrun simctl spawn "$udid" "$dir/tests/ink_tests" | tee "$dir/ink_tests.log"
