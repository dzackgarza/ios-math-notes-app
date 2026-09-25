# Traps

## Build / release

- **XcodeGen writes fixed `1.0` / `1` into Info.plist** when `info.properties` is used, ignoring `MARKETING_VERSION` / `CURRENT_PROJECT_VERSION`. `project.yml` must map `CFBundleShortVersionString: $(MARKETING_VERSION)` and `CFBundleVersion: $(CURRENT_PROJECT_VERSION)`. Symptom: SideStore "does not match the build number specified by the source".
- **Never publish the IPA at a reused URL.** A rolling `latest` tag let the iPad install a cached older IPA (build mismatch). Deleting a tag breaks every cached `source.json` that points at it ("MathNotes.ipa doesn't exist"). Each build gets its own tag `v<run>`; old releases are kept.
- **Size in `source.json` must be the exact IPA byte count.** SideStore verifies it. macOS `stat` is `stat -f %z`, not `-c %s`.
- **Workflow re-runs reuse `github.run_number`,** so the release step uploads with `--clobber` when the tag exists.

## Linux host

- **iloader AppImage aborts with `Could not create surfaceless EGL display: EGL_BAD_ALLOC`** on this machine. The AppImage bundles an old libwayland (upstream nab138/iloader#576); `WEBKIT_DISABLE_DMABUF_RENDERER`, `GDK_BACKEND=x11` do not help. Use the `.deb` asset's `usr/bin/iloader` against system `webkit2gtk-4.1`.
- **`usbmuxd` started after the iPad was plugged in does not see it.** Re-plug. `Pairing dialog response pending (-19)` means tap Trust on the iPad.

## SideStore on the iPad

- **Developer Mode switch is hidden** until a development-signed app (SideStore) is on the device and launched once.
- **"VPN connection error / no utun interface"**: LocalDevVPN is not connected.
- **Operation error 28** = `notAuthenticated` (Swift's implicit `CustomNSError` code for `OperationError`, payload cases first). Sign in inside SideStore's Settings; signing in to iloader is not enough.
- **A changed source is cached.** After changing download URLs, remove and re-add the source.

## Engine

- **The JetBrains/skia iOS prebuilts target iOS 12.0 and 14.0** (`otool -l` minos). The engine needs 18.0, so CI builds iOS Skia from source at the same commit (`core/scripts/build-skia-ios.sh`). The wasm prebuilt is used as shipped.
- **`SkPDF::MakeDocument` aborts the process** (`Must set both a jpegDecoder and jpegEncoder`) with a default `SkPDF::Metadata`. Start from `SkPDF::JPEG::MetadataWithCallbacks()` (`include/docs/SkPDFJpegHelpers.h`).
- **Headless Firefox on a runner without a GPU refuses every WebGL context** ("Exhausted GL driver options"; `webgl.force-enabled` does not help). CI runs Firefox headed under `xvfb-run`, where Mesa supplies GL.
- **`wasm-objdump -x` dumps data segments,** whose strings (`shared_ptr`, SkSL `atomicStore`) match a thread check. Inspect only `-j Memory` and `-j target_features`.
- **`actions/cache` rejects paths containing `..`** ("Relative pathing . and .. is not allowed") and then saves nothing, with only a warning. CI keeps its tool directories under `$GITHUB_WORKSPACE/.ci/`.
