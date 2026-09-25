# Architecture plan

One portable document and ink engine, with thin platform hosts. Write
([styluslabs/Write](https://github.com/styluslabs/Write), C++, AGPL-3.0) is the
reference implementation and the first engine. Its current UI and platform
structure is not the target architecture.

```text
                  write-core (C++)
   document/page model, SVG ink geometry, stroke building,
   selection, reflow, undo/redo, rendering, import/export
                         │  stable C ABI
        ┌────────────────┼────────────────┐
   iPadOS host       Linux host         Web host
   Swift/UIKit       SDL/OpenGL         JS + WASM
```

| Host | Role |
| --- | --- |
| Linux (SDL) | Development and reference host. Write's existing frontend; keep it working through every step. |
| Web (WASM) | Linux and browser host with no install. Modernize Write's Emscripten target (`Makefile.wasm`, `wasm/wasmhelper.c`). |
| iPadOS (UIKit) | Native host, not a WKWebView. Replaces and extends Write's `ios/ioshelper`. |

## Rules

- `core/` calls no platform API: no UIKit, SDL file dialogs, browser JS, X11.
- Swift and JS see only the C ABI: opaque handles plus plain structs. No C++
  classes cross the boundary.
- One renderer: Write's SVG/painter/NanoVG stack. The host supplies a drawable
  surface. A Metal renderer comes only if iPad latency measurements require it.
- One pointer record. UIKit Pencil events, browser Pointer Events, and Linux
  tablet events all normalize to it:
  `x, y, time, pressure, altitude, azimuth, roll, pointer kind, phase, predicted`.
  It extends Write's `InputPoint`. iOS feeds coalesced and predicted touches.
- Document format stays Write's SVG, round-trip compatible with Write, until
  the engine boundary is stable. A later container wraps the same pages:
  `Note.note/{manifest.json, pages/*.svg, assets/, index/}`.
- New features go in the engine or in a service, never in one host only.
  PDF and layers belong to the document model. OCR belongs to an indexing
  service. Scanner and microphone are host services.
- Write is the behavioral oracle. Before a change to reflow, ruled selection,
  free erase, line insertion, clipping, undo, or stroke serialization, record
  fixture documents and input-event traces with their resulting SVG. The new
  core must reproduce them.

## Steps

1. Fork Write. Build the current engine unchanged as `linux-x86_64`, `wasm`,
   and `ios-arm64`, all three in CI (Linux runners for Linux and WASM,
   macOS runner for iOS). Build modernization only; no redesign.
2. Extract the engine boundary: `Document`, `Page`, `Element`,
   `StrokeBuilder`, `Selection`, `SyncUndo`, SVG parse/write/render, and the
   geometric parts of `ScribbleView` and `ScribbleInput`.
3. Define the C ABI, for example:
   ```c
   InkDocument *ink_document_open(...);
   void ink_document_save(...);
   void ink_pointer_begin(InkCanvas *, const InkPointerEvent *);
   void ink_pointer_update(InkCanvas *, const InkPointerEvent *);
   void ink_pointer_end(InkCanvas *, const InkPointerEvent *);
   void ink_undo(InkDocument *);
   void ink_redo(InkDocument *);
   void ink_render(InkCanvas *, InkRenderTarget *);
   ```
4. Split `ScribbleApp` (document management, clipboard, preferences, UI, file
   operations, sync, export, lifecycle in one class) into core commands and
   host-supplied service interfaces:
   - Core: Document, Canvas, Commands, History, Renderer.
   - Services: Storage, Clipboard, PDF, Images, Search, Sync, Audio.
   - Host UI: iPadOS, SDL/Linux, Browser.
5. Web host storage: Emscripten IDBFS first, OPFS for the library later.
   Upload/download import and export always work; `showOpenFilePicker()` is
   an extra where available.
6. iPad host owns Files/`UIDocument`, share sheet, camera and scanner,
   keyboard text input, audio session, lifecycle, and Pencil interactions.
7. After Write parity, add the features in [FEATURES.md](FEATURES.md) in
   this order: PDF import and backgrounds, text elements, layers, metadata and
   tags, indexing and search, scanner, handwriting OCR, synchronized audio.

## Target layout

```text
core/      document/ ink/ reflow/ selection/ undo/ svg/ render/
services/  pdf/ search/ sync/
hosts/     ios/{Swift,CoreBridge}/  linux/SDL/  web/{wasm,shell}/
compat/    write/
tests/     documents/ input-traces/
.github/workflows/  linux.yml wasm.yml ios.yml
```

## Licensing

Copying Write's code makes this repository AGPL-3.0, and AGPL §13 applies to
the web host when it is served over a network. This repository is public and
the app is personal, so direct reuse is the plan. A non-AGPL product later
would need a clean reimplementation that uses Write only as a behavioral
reference.
