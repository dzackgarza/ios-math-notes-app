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
    uvx yamllint -s -d '{extends: relaxed, rules: {line-length: disable}}' project.yml .github/workflows/ios.yml .github/workflows/engine.yml .github/workflows/web.yml

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

# Rewrites core/tests/fixtures/templates: the notebook of each built-in template.
template-fixtures: engine-wasm
    node {{build}}/tests/write_templates.js core/tests/fixtures/templates

# Rewrites core/tests/fixtures/render: Chromium's rendering of each listed page in
# core/tests/fixtures/documents and core/tests/fixtures/templates.
render-goldens:
    cd core/tests/webgl && bun install --frozen-lockfile && bun run render-goldens.mjs

# Zoom and input timings in Chromium on this machine's GPU (not run in CI, which has none).
frame-times: engine-wasm
    cd core/tests/webgl && bun install --frozen-lockfile && WEBGL_CHECK_DIR=$PWD/../../build/wasm/tests/webgl bunx playwright test frame.spec.mjs -c playwright.config.mjs --project chromium-gpu --reporter list

# The web engine module (core/build/wasm/web/engine.mjs, .wasm, .d.mts) and its Node test build.
# --emit-tsd runs the tsc that hosts/web installs.
engine-module: engine-wasm
    cd hosts/web && bun install --frozen-lockfile
    PATH="$PWD/hosts/web/node_modules/.bin:$PATH" cmake --build {{build}} --target engine engine_test

# The TypeScript wrapper: type check against the module's --emit-tsd types, then the Node tests.
web-engine-test: engine-module
    mkdir -p hosts/web/src/engine/wasm
    cp {{build}}/web/engine.* {{build}}/web/engine_test.* hosts/web/src/engine/wasm/
    cd hosts/web && bunx tsc -b && node --test src/engine/engine.test.ts

# The web app in hosts/web/dist, with the engine module.
web-build: engine-module
    mkdir -p hosts/web/src/engine/wasm
    cp {{build}}/web/engine.* {{build}}/web/engine_test.* hosts/web/src/engine/wasm/
    cd hosts/web && bunx tsc -b && bunx --bun vite build

# Builds the web app and copies it to /var/www/math-notes (served at http://localhost/math-notes/, README).
web-deploy: web-build
    rsync -a --delete hosts/web/dist/ /var/www/math-notes/

# Vitest Browser Mode (Chromium and Firefox here; CI adds WebKit), then Playwright against the deployment.
web-test: web-deploy
    cd hosts/web && bunx vitest run --project chromium --project firefox
    cd hosts/web && bunx playwright test
