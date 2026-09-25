# Swift builds only on the macOS CI runner. The engine builds for wasm32 here.
# EMSDK must point at an activated emsdk 4.0.7; VCPKG_ROOT at a vcpkg clone.

emsdk := env("EMSDK", env("HOME") / ".cache/math-notes/emsdk")
vcpkg := env("VCPKG_ROOT", env("HOME") / ".cache/math-notes/vcpkg")
build := "core/build/wasm"

export EMSDK := emsdk
export PATH := emsdk / "upstream/emscripten" + ":" + env("PATH")

engine-wasm:
    cmake -S core -B {{build}} -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE={{vcpkg}}/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE={{emsdk}}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
      -DVCPKG_TARGET_TRIPLET=wasm32-emscripten -DVCPKG_MANIFEST_FEATURES=tests -DINK_BUILD_TESTS=ON
    cmake --build {{build}}

engine-test: engine-wasm
    cd {{build}} && ctest --output-on-failure
    qpdf --check {{build}}/tests/a4.pdf
    cd core/tests/webgl && bun install --frozen-lockfile && WEBGL_CHECK_DIR=$PWD/../../build/wasm/tests/webgl bunx playwright test -c playwright.config.mjs --project chromium --project firefox

test-commit:
    uvx yamllint -s -d '{extends: relaxed, rules: {line-length: disable}}' project.yml .github/workflows/ios.yml .github/workflows/engine.yml

test-push: test-commit

# Rewrites core/tests/fixtures/ink (traces and host outline goldens) on the Linux host.
ink-fixtures:
    cmake -S core/tools/ink-host -B core/build/ink-host -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE={{vcpkg}}/scripts/buildsystems/vcpkg.cmake
    cmake --build core/build/ink-host
    core/build/ink-host/ink_host_fixtures core/build/ink-host/_deps/google_ink-src core/tests/fixtures/ink

# Rewrites core/tests/fixtures/documents: the engine's output for each notebook in tests/documents.
document-fixtures: engine-wasm
    node {{build}}/tests/write_documents.js tests/documents core/tests/fixtures/documents full custom-size
