# Architecture plan

One portable document and ink engine (C++), with thin platform hosts.
[Stylus Labs Write](https://github.com/styluslabs/Write) is the behavioral
reference: it shows which behaviors to build and supplies test fixtures for
them. No Write code is used.

```text
                   ink engine (C++, also built to WASM)
   document/page model, stroke modeling, geometry,
   selection, reflow, undo/redo, rendering, import/export
                         │  stable C ABI
          ┌──────────────┼──────────────────┐
     Web host        iPadOS host         Linux dev host
     JS + WASM       Swift/UIKit         SDL, libinput
   primary on        full Apple Pencil   tests, raw
   Linux/Win/macOS   and lowest latency  tablet axes
```

| Host | Role |
| --- | --- |
| Web (WASM, PWA) | Primary product on desktop. Pointer Events give pen type, pressure, tilt, altitude/azimuth, buttons, hover, coalesced and predicted samples; Safari 18.2+ gives coalesced/predicted and altitude/azimuth, Safari 26.2 gives subpixel coordinates. |
| iPadOS (UIKit) | Native host, not a WKWebView. Needed for Pencil double-tap, Pencil Pro squeeze, barrel roll, hover pose and distance, haptics, and the shortest input-to-display path. WebKit still reports `twist` as 0 for Pencil Pro. |
| Linux native | Development executable: runs the fixture tests, and reads libinput/Wayland tablet axes (distance, rotation) that the web does not expose. Not a user-facing UI. |

## Dependencies

Write's stack is its own and about ten years old (`usvg`, `ulib`, `ugui`,
`nanovgXC`, a patched SDL), and its UI patterns are dated. The engine uses
mature libraries instead. Each one is confirmed by a build spike on all
three targets before the engine depends on it.

| Concern | Library | Notes |
| --- | --- | --- |
| Stroke modeling, brushes, stroke geometry, hit tests | [google/ink](https://github.com/google/ink) (C++, Apache-2.0) | Core of Android Jetpack Ink: smoothing, prediction, brush behaviors, mesh output, protobuf stroke storage. Android-first, Bazel build; iOS and WASM builds are unproven, and the API is not yet stable. |
| 2D rendering, text layout, SVG and PDF output | [Skia](https://skia.org) | Builds for Linux, iOS (Metal), and WASM (CanvasKit). |
| PDF page rendering | PDFium | Chromium's PDF renderer; draws through Skia. |
| Polygon operations (lasso, erase regions) | [Clipper2](https://github.com/AngusJohnson/Clipper2) | Only where google/ink geometry does not cover it. |

Reflow, insert space, and ruled select and erase have no library. They are
new engine code, specified by fixtures recorded from Write.

## Rules

- `core/` calls no platform API: no UIKit, SDL, browser JS, X11.
- Swift and JS see only the C ABI: opaque handles plus plain structs. No C++
  classes cross the boundary.
- One renderer: Skia. The host supplies a drawable surface (WebGL canvas,
  Metal layer, SDL window).
- One input record. Every host fills what its platform gives and sets a
  capability bit for it; missing values are absent, never invented.
  ```c
  typedef struct {
    double x, y, time;
    float pressure, altitude, azimuth, rotation, distance;
    uint32_t buttons;
    uint32_t has;        /* capability bits: which fields are real */
    InkTool tool;        /* pen, eraser, touch, mouse */
    InkPhase phase;      /* hover, begin, move, end, cancel */
    bool predicted;
  } InkPenSample;
  ```
  Sources: web `PointerEvent` + `getCoalescedEvents()`/`getPredictedEvents()`;
  UIKit `UITouch` coalesced/predicted touches, `UIPencilInteraction`,
  `UIPencilHoverPose`; Linux libinput / Wayland `tablet-v2`.
- Pages are SVG, so notes stay open vector files. Write documents import.
  A note is a directory: `Note.note/{manifest.json, pages/*.svg, assets/, index/}`.
- New features go in the engine or in a service, never in one host only.
  PDF and layers belong to the document model. OCR belongs to an indexing
  service. Scanner and microphone are host services.
- Write fixtures: documents and input-event traces with their resulting SVG,
  for reflow, ruled selection, free erase, line insertion, clipping, undo,
  and stroke serialization. The engine must reproduce the behavior.

## Steps

1. Build Write on the Linux dev host and record the fixtures and traces.
2. Build spikes: google/ink, Skia, PDFium, Clipper2 for `linux-x86_64`,
   `wasm`, and `ios-arm64`, in CI (Linux runners for Linux and WASM, macOS
   runner for iOS).
3. Engine: document model, strokes, selection, undo, rendering, then reflow
   and ruled operations against the fixtures.
4. C ABI:
   ```c
   InkDocument *ink_document_open(...);
   void ink_document_save(...);
   void ink_input(InkCanvas *, const InkPenSample *, size_t count);
   void ink_undo(InkDocument *);
   void ink_redo(InkDocument *);
   void ink_render(InkCanvas *, InkRenderTarget *);
   ```
5. Web host: canvas, Pointer Events adapter, storage on Emscripten IDBFS
   first and OPFS for the library later. Upload/download import and export
   always work; `showOpenFilePicker()` is an extra where available.
6. iPad host: Files/`UIDocument`, share sheet, camera and scanner, keyboard
   text input, audio session, lifecycle, and Pencil interactions. Replaces
   the current SwiftUI placeholder; the SideStore release pipeline stays.
7. Services the hosts supply: Storage, Clipboard, PDF, Images, Search, Sync,
   Audio.
8. After Write parity, add the features in [FEATURES.md](FEATURES.md) in
   this order: PDF import and backgrounds, text elements, layers, metadata and
   tags, indexing and search, scanner, handwriting OCR, synchronized audio.

## Target layout

```text
core/      document/ strokes/ reflow/ selection/ undo/ render/ io/
services/  pdf/ search/ sync/
hosts/     web/{wasm,shell}/  ios/{Swift,CoreBridge}/  linux/
tests/     documents/ input-traces/
.github/workflows/  linux.yml wasm.yml ios.yml
```
