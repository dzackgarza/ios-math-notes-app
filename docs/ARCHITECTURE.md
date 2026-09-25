# Architecture plan

One portable document and ink engine (C++), with two platform hosts: a web
app and an iPad app. This repository is the monorepo for all of it.
[Stylus Labs Write](https://github.com/styluslabs/Write) is the behavioral
reference: it shows which behaviors to build and supplies test fixtures for
them. No Write code is used.

```text
            ink engine (C++, built to WASM and to iOS arm64)
   document/page model, stroke modeling, geometry,
   selection, reflow, undo/redo, rendering, import/export
                         │  stable C ABI
                ┌────────┴─────────┐
            Web host           iPadOS host
            JS + WASM          Swift/UIKit
```

| Host | Role |
| --- | --- |
| Web (WASM, PWA) | Built first. The product on Linux, Windows, and macOS. Pointer Events give pen type, pressure, tilt, altitude/azimuth, buttons, hover, coalesced and predicted samples; Safari 18.2+ gives coalesced/predicted and altitude/azimuth, Safari 26.2 gives subpixel coordinates. |
| iPadOS (UIKit) | Built second. Native host, not a WKWebView. Needed for Pencil double-tap, Pencil Pro squeeze, barrel roll, hover pose and distance, haptics, and the shortest input-to-display path. WebKit still reports `twist` as 0 for Pencil Pro. |

## Dependencies

Write's stack is its own and about ten years old (`usvg`, `ulib`, `ugui`,
`nanovgXC`, a patched SDL), and its UI patterns are dated. The engine uses
mature libraries instead. Each one is confirmed by a build spike on both
targets before the engine depends on it.

### Engine (C++20, one source for WASM and iOS)

| Concern | Library | Notes |
| --- | --- | --- |
| Input smoothing and prediction | [google/ink-stroke-modeler](https://github.com/google/ink-stroke-modeler) | Apache-2.0, CMake, made for handwriting. |
| Brush outline, stroke mesh, hit tests | [google/ink](https://github.com/google/ink) | Uses ink-stroke-modeler itself. Android-first, Bazel, unstable API: adopted only if the spike builds it for iOS and WASM. |
| 2D rendering and PDF export | [Skia](https://skia.org) | PDF export through its PDF backend. Linked into the engine on both targets: WebGL/WebGPU in the WASM build, Metal on iOS. The web host calls the engine, not CanvasKit, so one render path serves both hosts. |
| PDF import | [MuPDF](https://mupdf.com) | Renders PDF pages to PNG backgrounds at import. Used nowhere else. |
| Polygon operations (lasso, erase regions) | [Clipper2](https://github.com/AngusJohnson/Clipper2) | Only where google/ink geometry does not cover it. |
| Tests | Catch2 | Engine unit tests and trace tests, run in the WASM build under Node in CI. |

### Hosts

| Host | Stack |
| --- | --- |
| Web | TypeScript, SolidJS for chrome (toolbars, library, panels), Vite (run with bun). Pointer events go straight to the engine; no pen sample passes through Solid state. Playwright for Chrome, Firefox, WebKit tests. |
| iPad | SwiftUI for chrome and library; UIKit view with a Metal layer for the canvas. Apple frameworks: UniformTypeIdentifiers, `UIDocument`. Swift Observation for shell state. |

### Added with the feature that needs it

| Feature | Library |
| --- | --- |
| Import of arbitrary SVG clippings | [resvg](https://github.com/linebender/resvg) `usvg`, through its C API, to normalize foreign SVG. |

Reflow, insert space, and ruled select and erase have no library. They are
new engine code, specified by fixtures recorded from Write.

## Rules

- `core/` calls no platform API: no UIKit, browser JS.
- Swift and JS see only the C ABI: opaque handles plus plain structs. No C++
  classes cross the boundary.
- One renderer: Skia, inside the engine. The host supplies a drawable surface
  (WebGL canvas, Metal layer).
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
  `UIPencilHoverPose`.
- Pages are discrete, fixed-size, and printable: one notebook page is one
  printed page. A4 by default; the size is a notebook setting. There is no
  infinite canvas. Reflow and insert space that push ink past the bottom of
  a page move it onto the next page, adding a page when needed.
- Pages are standalone SVG files in a notebook directory.
  Storage and file format: [FORMAT.md](FORMAT.md).
- New features go in the engine or in a service, never in one host only.
  Layers belong to the document model; PDF import is an engine function.
- Write fixtures: documents and input-event traces with their resulting SVG,
  for reflow, ruled selection, free erase, line insertion, clipping, undo,
  and stroke serialization. The engine must reproduce the behavior.

## Steps

1. Run the Write app on Linux and record the fixtures and traces, using the
   comparison corpus in FORMAT.md as sample documents.
2. Build spikes: ink-stroke-modeler, google/ink, Skia, MuPDF, Clipper2 for
   `wasm` and `ios-arm64` in CI (Linux runner for WASM, macOS runner for iOS).
3. Engine: document model, fixed pages, strokes, selection, undo, rendering,
   then reflow and ruled operations against the fixtures.
4. C ABI:
   ```c
   InkDocument *ink_document_open(...);
   void ink_document_save(...);
   void ink_input(InkCanvas *, const InkPenSample *, size_t count);
   void ink_undo(InkDocument *);
   void ink_redo(InkDocument *);
   void ink_render(InkCanvas *, InkRenderTarget *);
   ```
5. Local test deployment, used from step 6 on: a static build copied to `/var/www/math-notes`
   and served by the local nginx at `http://localhost/math-notes/`. The
   Firefox WebDAV bridge then runs with
   `rclone serve webdav ~/Notes --addr 127.0.0.1:31415 --allow-origin http://localhost`.
6. Web host: canvas, Pointer Events adapter, and the notebook-root access
   modes in FORMAT.md. Upload/download import and export always work.
7. iPad host: Files/`UIDocument`, share sheet, lifecycle, and Pencil
   interactions. Replaces the current SwiftUI placeholder; the SideStore
   release pipeline stays.
8. Services the hosts supply: Storage (the notebook root), Clipboard, Images.
9. After Write parity, add the features in [FEATURES.md](FEATURES.md) in
   their listed order.

## Target layout

```text
core/      document/ strokes/ reflow/ selection/ undo/ render/ io/
services/  storage/
hosts/     web/{wasm,shell}/  ios/{Swift,CoreBridge}/
tests/     documents/ input-traces/
.github/workflows/  wasm.yml ios.yml
```
