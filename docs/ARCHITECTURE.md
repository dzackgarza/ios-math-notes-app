# Architecture

One portable document and ink engine (C++20), with two platform hosts: a web
app and an iPad app. This repository is the monorepo for all of it.
[Stylus Labs Write](https://github.com/styluslabs/Write) is a reference for
ink editing and its behavior fixtures. Select reusable owners for those
capabilities under the component-ownership rules below. The portable engine
integrates those owners with the notebook format and the platform hosts.

Write is a source of algorithms, code patterns, features and extension
points, and fixtures check behavior, never appearance. The look and the
everyday interaction patterns (page layout, scrolling, adding pages, tool
chrome, colors, paper) follow GoodNotes and Noteful and the tablet spec
([specs/tablet-ui.md](specs/tablet-ui.md)); nothing visual is taken from
Write.

Work order and milestones: the GitHub issue tree rooted at
[#11](https://github.com/dzackgarza/math-notes-app/issues/11)
(`itree next dzackgarza/math-notes-app` gives the next work unit).

```text
            ink engine (C++20, built to WASM and to iOS arm64)
   document model, stroke building, geometry, selection, reflow,
   undo/redo, rendering, page SVG read/write, PDF export
                         │  stable C ABI (core/include/ink.h)
                ┌────────┴─────────┐
            Web host           iPadOS host
     TypeScript + WASM         Swift, UIKit, Metal
```

| Host | Role |
| --- | --- |
| Web (WASM, PWA) | Built first. The product on Linux, Windows, and macOS, in desktop Chrome. |
| iPadOS (UIKit) | Built second. Native host, not a WKWebView. Needed for Pencil double tap, Pencil Pro squeeze and barrel roll, hover pose, haptics, and the shortest input-to-display path. |

## Engine integration

These boundaries describe the planned C++/host integration. A component
ownership decision must assess changes to them when a complete editor or SDK
can own more behavior. Architecture choices remain subject to that assessment;
the source-preservation and notebook behavior contracts remain requirements.

- `core/` calls no platform API and does no file I/O. The host reads files
  and passes their bytes to the engine. The engine returns the bytes of each
  file that changed, and the host writes them. Storage, clipboard, and PDF
  rasterization are host services.
- Swift and TypeScript see only the C ABI: opaque handles plus plain structs
  with fixed layouts. No C++ type crosses the boundary. The header has a
  `static_assert(offsetof(...))` for every struct field, and the TypeScript
  wrapper mirrors the same offsets.
- One renderer: Skia inside the engine, Ganesh backend, WebGL2 in WASM and
  Metal on iOS. The host supplies the surface (a canvas element, a
  `CAMetalLayer`). Strokes are drawn as filled `SkPath`s built from the same
  outline walk that writes the SVG `d`.
- One input record. Every host fills what its platform measures and sets a
  capability bit for it. The bits come from the platform, never from the
  values: the web reports 0.5 pressure and 0 twist when the hardware has no
  sensor.
  ```c
  typedef struct {
    double x, y;          /* view coordinates: CSS px, UIKit points */
    double time;          /* ms, monotonic clock of the host */
    float pressure;       /* 0..1 */
    float altitude;       /* rad, 0 = parallel to the screen */
    float azimuth;        /* rad */
    float roll;           /* rad: Pencil Pro rollAngle, web twist */
    float hover_height;   /* 0..1, UIKit zOffset; iPad only */
    uint32_t buttons;     /* web buttons bits; 32 = eraser */
    uint32_t has;         /* INK_HAS_* capability bits */
    uint32_t id;          /* host sample id, for later updates */
    uint8_t tool;         /* InkTool: pen, eraser, touch, mouse */
    uint8_t phase;        /* InkPhase: hover, begin, move, end, cancel */
    uint8_t predicted;    /* 1 for a predicted sample */
    uint8_t reserved;
  } InkPenSample;
  ```
  `ink_input(canvas, samples, n)` takes a batch per platform event.
  `ink_input_update(canvas, samples, n)` replaces the values of earlier
  samples with the same `id`: UIKit sends the final force and angles later,
  through `touchesEstimatedPropertiesUpdated`, sometimes after the touch
  ends.
- Pages are discrete, fixed-size, and printable: one notebook page is one
  printed sheet. A4 by default; the size is a notebook setting, and an
  imported PDF page keeps its own size. There is no infinite canvas. Reflow
  and insert space that push ink past the bottom of a page move it onto the
  next page and add a page when needed.
- Storage and file format: [FORMAT.md](FORMAT.md).
- Document state is an immutable value (immer). A maintained history component
  owns undo and redo; select it in #22. The document adapter supplies command
  grouping and saved-state identity. A page is dirty when its value differs
  from the last saved document.
- New features go in the engine or in a host service that both hosts
  supply, never in one host only. Layers belong to the document model.
- Every custom implementation links its ownership decision from the source.
  A justified upstream adaptation also cites the source file, symbol, pinned
  commit, and license.

## Component ownership

### Philosophy and invariants

Math Notes composes mature application components around research notes.
The project defines the notebook's meaning: page and object identity,
mathematical relationships, durable editable source, and the connection
between note ink and TikZ figures. Established libraries and platform services
own the mechanisms that present, edit, render, and store that content.

Navigation, tabs, text editing and layout, drag-and-drop, and other common app
behavior belong to complete framework components. Use their interaction,
accessibility, input, and lifecycle contracts together with their appearance.
The product specification describes the user's task and any real departure
from those contracts. Standard behavior is inherited from the owner; observed
examples are not a substitute for that contract.

Ink reflow is an ink-editor capability. Its use in mathematical notes does
not make its implementation project-specific. The same dependency search
applies to reflow, selection, recognition, persistence, and TikZ tooling.
Even a mathematical data model requires evidence before it gains a new
custom mechanism. Folder layout and source-preservation requirements justify
format mappings; they do not confer ownership of XML parsing, text shaping,
file coordination, or general editor infrastructure.

Custom code is limited to a demonstrated product rule or the smallest adapter
needed to connect selected owners. An adapter translates representations or
commands at a named boundary. A replacement interaction controller, parser,
layout engine, solver, or history implementation is a subsystem, regardless
of its size, filename, or description as glue.

Before writing or extending any custom behavior, its owning issue must contain
an **ownership decision** with the following evidence. One decision may cover
the functions within one stated boundary; each function must stay within it.

| Required evidence | Content |
| --- | --- |
| Requirement | The exact product outcome, its source, and the data that must survive it. Separate user requirements from assumptions introduced by existing code or plans. |
| Search record | Date, actual search queries, sources searched, and links to primary documentation, APIs, source, and working examples inspected. Search for complete applications, embeddable editors, SDKs, frameworks, and maintained forks as well as small packages. |
| Candidate assessment | For each credible owner, record supported behavior, extension points, version, maintenance evidence, license, host support, offline operation, and source/data fidelity. Consider large dependencies and commercial SDKs. Size, unfamiliarity, or mismatch with the current architecture alone cannot reject a candidate. |
| Gap evidence | Cite the documented restriction or a reproducible integration result for each rejection. Distinguish an untested fit from an unsupported capability. Explain why configuration, composition, a plugin, or an upstream extension cannot satisfy the requirement. |
| Necessary local ownership | State why Math Notes must own the remaining behavior, rather than its library or fork. Name the smallest custom operation, its inputs and outputs, and the behavior still owned upstream. An existing implementation, issue, fixture, or reference algorithm does not establish this necessity. |
| Core scope | Explain how the operation follows from the mathematical note model or source-preservation contract. Any additional responsibility needs an explicit architecture decision and user approval before implementation. |
| Decision and proof | Name the selected owner and version pin, the adapter boundary, integration acceptance on supported hosts, and the approval for any custom subsystem. A justified port also needs upstream provenance and license. |

A search with unresolved fit questions permits further evaluation, not a local
replacement. A dependency that covers the requirement owns the whole behavior.
Any exception requires the completed evidence above and explicit user approval.
Keep the decision with the issue and link it here. Revisit it when a proposed
change expands the boundary. This rule applies equally to new code, extensions
of existing code, copied examples, and project-maintained forks.

### Integration targets

These are required ownership boundaries for the plan, not claims that every
current call site already uses them. A selection prerequisite must be resolved
before implementation of the affected capability.

| Concern | Owner and Math Notes boundary |
| --- | --- |
| Web page scrolling | [Ionic `IonContent`](https://ionicframework.com/docs/api/content) and its browser-native scroll element own navigation. The renderer consumes the resulting viewport. Pen sample capture is separate from finger navigation. |
| iPad page navigation | [`UIScrollView`](https://developer.apple.com/documentation/uikit/uiscrollview) owns scrolling and zoom around the Metal drawing surface. UIKit arbitrates direct touches and Pencil input. |
| Web document zoom and page-end insertion affordance | #21 must select a complete supported interaction component that coexists with native scrolling. Include full editor frameworks in the survey. Math Notes receives the completed zoom or add-page command. |
| Chrome, sheets, menus, and forms | Ionic components on the web; SwiftUI and UIKit on iPad. Use their full control behavior and accessibility. App code supplies content and document commands. |
| Document tabs | #62 selects a complete accessible tabs component; [Kobalte Tabs](https://kobalte.dev/docs/core/components/tabs/) is a Solid-compatible candidate. The app maps selected tabs to notes and preserves each note's state. |
| Typed text | Native browser editing controls and UIKit text controls own input, composition, caret, and selection. [Skia Paragraph](https://skia.org/docs/user/modules/quickstart/) is the engine layout candidate for #61; verify shaping, line layout, font metrics, and SVG fidelity before adoption. |
| Drag-and-drop and selection manipulation | [UIKit drag-and-drop](https://developer.apple.com/documentation/uikit/drag-and-drop) owns native transfers. #24 and #33 select a mature touch-capable web editor/interaction component. The document adapter applies the completed operation to selected objects. |
| Split panes | Host pane and scroll components own layout and navigation. #28 connects their position callbacks and document commands. |
| History | #22 selects a maintained immutable-history component. The app supplies notebook action boundaries and saved-state identity. |
| Ink and reflow | google/ink owns the documented brush and geometry capabilities used by the engine. #30 and #31 select the owner of ink structure, ruled editing, and reflow; see the [reflow survey](#ink-reflow-owner-survey). |
| Persistence and offline lifecycle | File System Access, IndexedDB/idb-keyval, Apple file coordination, and Vite PWA/Workbox own their respective platform mechanisms. #3, #5, and #7 define the minimum notebook-format and save-transaction adapters, including interruption and conflict behavior. |
| Source syntax and graphics | pugixml, JSON parsers, Skia, and the selected source editor/parser own syntax and rendering. The app supplies the SVG/InkML mapping and preserves authored source. |
| Mathematical figures | The [FreeTikZ integration plan](specs/tikz-drawing-mode.md#component-ownership) selects owners for vector editing, geometry, constraints, source editing, and TeX. The app owns the link from captured note ink to the editable figure. |

### Ink reflow owner survey

Reflow ownership is under evaluation in #30 and #31. The evidence and required
fit checks are recorded in [ink-reflow-owners.md](ink-reflow-owners.md).
Write remains a behavior reference while complete SDK and upstream-component
integration options are compared.

## Dependencies

### Engine

| Concern | Library | Pin and acquisition | Introduced in |
| --- | --- | --- | --- |
| Brushes, stroke input smoothing, stroke outlines, hit tests, lasso coverage | [google/ink](https://github.com/google/ink) (Apache-2.0) | Commit `1b220eee` (2026-09-23). Built from source by the engine's CMake, from a source list that follows Chromium's `third_party/ink/BUILD.gn`. Modules: `brush`, `color`, `geometry`, `strokes`, `types`. | [#18](https://github.com/dzackgarza/math-notes-app/issues/18) |
| google/ink dependencies | abseil-cpp, libtess2 | abseil `20260526.0`, libtess2 `446bae6`: the pins in google/ink's `MODULE.bazel` | [#18](https://github.com/dzackgarza/math-notes-app/issues/18) |
| 2D rendering, PDF export | [Skia](https://skia.org) | Prebuilt [JetBrains/skia](https://github.com/JetBrains/skia) release `m154-ab5932137b`: the `wasm` zip and the `ios-arm64` and `iosSim-arm64` zips, each pinned by SHA-256 | [#2](https://github.com/dzackgarza/math-notes-app/issues/2) |
| Page XML read and write | pugixml 1.16 | vcpkg | [#3](https://github.com/dzackgarza/math-notes-app/issues/3) |
| Number parsing | fast_float 8.3.0 | vcpkg | [#3](https://github.com/dzackgarza/math-notes-app/issues/3) |
| Number formatting | `std::to_chars` (fixed precision) | libc++ (iOS 16.3+, Emscripten) | [#3](https://github.com/dzackgarza/math-notes-app/issues/3) |
| Immutable document values | immer 0.9.1 | vcpkg | [#3](https://github.com/dzackgarza/math-notes-app/issues/3) |
| Free-erase interval union | boost-icl (Boost.Icl `interval_set`) 1.92 | vcpkg | [#23](https://github.com/dzackgarza/math-notes-app/issues/23) |
| Lasso simplification (Ramer-Douglas-Peucker) | boost-geometry (Boost.Geometry `simplify`) 1.92 | vcpkg | [#24](https://github.com/dzackgarza/math-notes-app/issues/24) |
| Engine tests | Catch2 3.16.0 | vcpkg; tests run in the WASM build under Node | [#2](https://github.com/dzackgarza/math-notes-app/issues/2) |
| InkML compatibility check of written pages | Wacom [universal-ink-library](https://github.com/Wacom-Developer/universal-ink-library) 2.1.1 (`InkMLParser`) | PyPI, run with `uvx` in CI; test-only | [#3](https://github.com/dzackgarza/math-notes-app/issues/3) |

Toolchain: emsdk 4.0.7 for every WASM object (Emscripten has no ABI
stability between versions, and the Skia prebuilt uses 4.0.7); Xcode on the
`macos-26` runner; CMake with Ninja (`CMAKE_SYSTEM_NAME=iOS` for iOS); vcpkg
in manifest mode with a pinned `builtin-baseline` and overlay triplets for
`wasm32-emscripten` and `arm64-ios`. WASM is single-threaded, so the web host
needs no COOP/COEP headers.

### Web host

| Concern | Library | Introduced in |
| --- | --- | --- |
| UI chrome (toolbars, library, panels) | SolidJS 1.9, [Ionic](https://ionicframework.com/docs/components) 8 web components in iOS mode, so the web chrome matches the iPad host's SwiftUI controls, through the Solid components of [@ionic-solidjs/core](https://github.com/ionic-solidjs/ionic-solidjs); tool icons from lucide-solid | [#57](https://github.com/dzackgarza/math-notes-app/issues/57) |
| Build, dev server, PWA | Vite 8 (run with `bunx --bun vite`), vite-plugin-pwa 1.3 | [#5](https://github.com/dzackgarza/math-notes-app/issues/5) |
| Folder handle persistence (Chromium) | idb-keyval 6.3 | [#5](https://github.com/dzackgarza/math-notes-app/issues/5) |
| PDF page rasterizer | [mupdf](https://www.npmjs.com/package/mupdf) (Artifex's WASM build), in a Web Worker, loaded only at import | [#8](https://github.com/dzackgarza/math-notes-app/issues/8) |
| Tests | Vitest 5 Browser Mode with the Playwright 1.63 provider (Chromium) | [#5](https://github.com/dzackgarza/math-notes-app/issues/5) |

Pen samples go straight to the engine; no pen sample passes through Solid
state. Platform navigation stays with the host's interaction components.

### iPad host

| Concern | API | Introduced in |
| --- | --- | --- |
| UI chrome, library | SwiftUI, Swift Observation | [#7](https://github.com/dzackgarza/math-notes-app/issues/7) |
| Canvas and navigation | `UIScrollView` containing a UIKit view with a `CAMetalLayer`; frame timing by `UIUpdateLink` (iOS 18) | [#7](https://github.com/dzackgarza/math-notes-app/issues/7) |
| Notebook root | `UIDocumentPickerViewController` for folders, security-scoped bookmarks, `NSFileCoordinator`, one `NSFilePresenter` on the root, `NSFileVersion` for iCloud edit conflicts | [#7](https://github.com/dzackgarza/math-notes-app/issues/7), [#34](https://github.com/dzackgarza/math-notes-app/issues/34) |
| PDF intake and rasterizer | `CFBundleDocumentTypes` for `com.adobe.pdf` (Files "Open in" and the share sheet), PDFKit | [#8](https://github.com/dzackgarza/math-notes-app/issues/8) |
| Engine linkage | `InkEngine.xcframework` from the CMake build, linked by XcodeGen with `embed: false` and `libc++.tbd` | [#2](https://github.com/dzackgarza/math-notes-app/issues/2) |

## Reference implementations

Use these sources to evaluate behavior and integration. Each work-unit issue
must establish ownership before authorizing an adaptation. Source availability
alone is not a decision to copy its implementation.

| Engine or host concern | API or behavior reference | License |
| --- | --- | --- |
| Reflow, ruled insert space | Write `syncscribble/selection.cpp` `Selection::reflowStrokes`, `Selection::insertSpace`; tool glue in `syncscribble/scribblearea.cpp` `doPressEvent`/`doMoveEvent`/`doReleaseEvent` (`MODE_INSSPACERULED`) | AGPL-3.0 |
| Vertical and horizontal insert space | Write `scribblearea.cpp` (`MODE_INSSPACEVERT`, `MODE_INSSPACEHORZ`), `RectSelector` | AGPL-3.0 |
| Ruled select, ruled erase, column detection | Write `selection.cpp` `RuledSelector::selectRuled`, `findStops`, `containedRuled`, `overlapRuled`, `RuledRange` | AGPL-3.0 |
| Line assignment of strokes | Write `strokebuilder.cpp` `calcCom`, `scribblearea.cpp` `groupStrokes`, `page.cpp` `getLine` | AGPL-3.0 |
| Free erase (split strokes) | Write `element.cpp` `Element::freeErase`, `erasePenPoints`, `getEraseSubPaths`; Xournal++ `src/core/model/eraser/ErasableStroke.cpp` (interval union over the centerline) | AGPL-3.0, GPL-2.0+ |
| Stroke eraser | google/ink `Intersects(PartitionedMesh, Quad)`; Jetpack Ink geometry guide; Google's Cahier sample `DrawingCanvasViewModel.kt` | Apache-2.0 |
| Lasso select | google/ink `geometry_internal::CreateClosedShape`, `CreateMeshFromPolyline`, `PartitionedMesh::CoverageIsGreaterThan`, as in `ink/strokes/internal/jni/mesh_creation_native.cc`; lasso point handling from Write `LassoSelector::addPoint` | Apache-2.0, AGPL-3.0 |
| Selection transform semantics | Write `selection.cpp` `Selection::translate`/`scale`/`commitTransform`; the selected editor component owns manipulation controls | AGPL-3.0 |
| Stroke outline to SVG `d` and `SkPath` | google/ink `ink/rendering/skia/native/internal/path_drawable.cc`; Chromium `pdf/pdfium/pdfium_ink_writer.cc` (outline walk, nonzero fill) | Apache-2.0, BSD-3 |
| InkML trace text and `traceFormat` | W3C InkML Recommendation §3; microsoft/InkMLjs `InkMLjs/inkml.js` (`InkTrace`, `InkTraceFormat`); checked against Wacom universal-ink-library `uim/codec/parser/inkml.py` | Apache-2.0 |
| google/ink without Bazel | Chromium `third_party/ink/BUILD.gn` (source list) | BSD-3 |
| Immutable history candidate | [lager](https://github.com/arximboldi/lager); #22 evaluates maintained component integration with immer | MIT |
| Page ruling and templates | Write `page.cpp` `Page::generateRuleLayer`, `rulingdialog.cpp` presets | AGPL-3.0 |
| Bookmarks and links | Write `scribblearea.cpp` (`MODE_BOOKMARK`, hyperref groups), `page.cpp` `Page::getHyperRef`, `bookmarkview.cpp` | AGPL-3.0 |
| Clipping data and insertion semantics | Write `clippingview.cpp`; host components own panel controls and drag-and-drop | AGPL-3.0 |
| TikZ drawing editor | [FreeTikZ](https://github.com/chrisheunen/freetikz) for pen-first capture; the [drawing-mode specification](specs/tikz-drawing-mode.md) defines the scene, source, and page integration | MIT |
| PDF export, link annotations | Skia `docs/examples/PDF.cpp`, `include/core/SkAnnotation.h` | BSD-3 |
| WebGL surface | Skia `modules/canvaskit/canvaskit_bindings.cpp` (`MakeGrContext`, `MakeOnScreenGLSurface`) | BSD-3 |
| Metal surface | Skia `tools/window/ios/MetalWindowContext_ios.mm`, `SkSurfaces::WrapCAMetalLayer` | BSD-3 |
| Catch2 under Node | adobe/lagrange `cmake/lagrange/lagrange_add_executable.cmake`, `lagrange_add_test.cmake` | Apache-2.0 |
| Pointer Events adapter | W3C Pointer Events Level 3, coalesced and predicted events | — |
| Pencil input | Apple sample "Leveraging touch input for drawing apps" | — |
| iPad folder access | Apple article "Providing access to directories" | — |
| Sync conflict names | Nextcloud desktop `src/common/utility.cpp` `makeConflictFileName`; Syncthing `lib/model/folder_sendrecv.go` `conflictName`; Apple TN2336 | — |

Pinned commits: Write `401b65d`, google/ink `1b220eee`, Xournal++ `b8b3a59`.
The repository license is AGPL-3.0-or-later, which
admits every source above.

## Target layout

```text
core/      include/ink.h  src/{document,format,strokes,geometry,layout,selection,
           ruled,reflow,undo,render,export}/  tests/
hosts/     web/  ios/
tests/     fixtures/write/   (traces and expected results recorded from Write)
.github/workflows/  engine.yml  web.yml  ios.yml
```
