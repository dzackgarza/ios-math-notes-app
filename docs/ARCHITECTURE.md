# Architecture plan

One portable document and ink engine (C++), with thin platform hosts.
[Stylus Labs Write](https://github.com/styluslabs/Write) is the reference
implementation and the behavioral oracle. Its app layer and UI are not reused.

```text
                   ink engine (C++, also built to WASM)
   document/page model, SVG ink geometry, stroke building,
   selection, reflow, undo/redo, rendering, import/export
                         │  stable C ABI
          ┌──────────────┼──────────────────┐
     Web host        iPadOS host         Linux dev host
     JS + WASM       Swift/UIKit         SDL, libinput
   primary on        full Apple Pencil   reference build,
   Linux/Win/macOS   and lowest latency  tests, raw tablet axes
```

| Host | Role |
| --- | --- |
| Web (WASM, PWA) | Primary product on desktop. Pointer Events give pen type, pressure, tilt, altitude/azimuth, buttons, hover, coalesced and predicted samples; Safari 18.2+ gives coalesced/predicted and altitude/azimuth, Safari 26.2 gives subpixel coordinates. |
| iPadOS (UIKit) | Native host, not a WKWebView. Needed for Pencil double-tap, Pencil Pro squeeze, barrel roll, hover pose and distance, haptics, and the shortest input-to-display path. WebKit still reports `twist` as 0 for Pencil Pro. |
| Linux native | Development and reference executable: runs the oracle tests, and reads libinput/Wayland tablet axes (distance, rotation) that the web does not expose. Not a user-facing UI. |

## Reuse or rewrite

Measured on Write `master` (2026-06-23). The decision is per layer.

| Layer | Write source | Coupling | Decision |
| --- | --- | --- | --- |
| Document model | `document`, `page`, `element`, `selection`, `strokebuilder`, `syncundo` (~5.4k lines) | Includes only pugixml, `ulib`, `usvg`. No ugui, no SDL, no `ScribbleApp`. | Take as the engine core. Already portable. |
| SVG and rendering | `usvg` (~6.9k), `ulib` (~5.8k), `nanovgXC` (~14.9k) | Standalone libraries. | Use as dependencies from their upstream repos. |
| Editing operations | `scribblearea` (~3k: reflow, insert space, ruled select/erase), `scribbledoc` (~1k), `scribbleview`, `scribblemode` | Include `scribbleapp.h` or `scribblewidget.h`; `scribbledoc` calls `ScribbleApp::openURL`. | Move into the engine and cut the app calls behind the C ABI. Reflow is the hardest behavior to reproduce; do not rewrite it. |
| Input | `scribbleinput` | Depends on `ugui`. | Rewrite against the `PenSample` record below. |
| App and UI | `scribbleapp` (~3k), `mainwindow`, `documentlist`, dialogs, toolbars, `ugui` | UI toolkit and platform. | Do not reuse. Each host builds its own UI. |

Result: neither "fork and gut" nor "rewrite". Start a new repository
structure, copy the engine-layer files from Write into `core/` with their
history reference, and write the hosts new. Copied files are owned code, not
a vendored library, so they are edited freely.

## Rules

- `core/` calls no platform API: no UIKit, SDL, browser JS, X11.
- Swift and JS see only the C ABI: opaque handles plus plain structs. No C++
  classes cross the boundary.
- One renderer: `usvg` + nanovgXC. The host supplies a drawable surface
  (WebGL canvas, `MTKView`/GL layer, SDL window). A Metal renderer comes only
  if iPad latency measurements require it.
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
- Document format is Write's SVG, round-trip compatible with Write, until
  the engine boundary is stable. A later container wraps the same pages:
  `Note.note/{manifest.json, pages/*.svg, assets/, index/}`.
- New features go in the engine or in a service, never in one host only.
  PDF and layers belong to the document model. OCR belongs to an indexing
  service. Scanner and microphone are host services.
- Write is the behavioral oracle. Record fixture documents and input-event
  traces with their resulting SVG from Write for reflow, ruled selection,
  free erase, line insertion, clipping, undo, and stroke serialization. The
  engine must reproduce them.

## Steps

1. Build Write unchanged on the Linux dev host and record the oracle
   fixtures and traces.
2. Create `core/` from the engine-layer files and the `usvg`, `ulib`,
   `nanovgXC` dependencies. Build it for `linux-x86_64`, `wasm`, and
   `ios-arm64` in CI (Linux runners for Linux and WASM, macOS runner for iOS).
3. Move the editing operations out of `scribblearea`/`scribbledoc` into the
   engine, and pass the oracle tests.
4. Define the C ABI:
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
core/      document/ ink/ reflow/ selection/ undo/ svg/ render/
services/  pdf/ search/ sync/
hosts/     web/{wasm,shell}/  ios/{Swift,CoreBridge}/  linux/
tests/     documents/ input-traces/
.github/workflows/  linux.yml wasm.yml ios.yml
```
