# Feature spec

Math Notes is a vector handwriting app for mathematics notes and talks. Its
features come from [Stylus Labs Write](https://github.com/styluslabs/write):
notes are SVG files on the ordinary filesystem, and ink is reflowable. Its
look and everyday interaction follow GoodNotes and Noteful, as specified in
[specs/tablet-ui.md](specs/tablet-ui.md); nothing visual comes from Write. Because
the files are plain SVG, external programs can process them; the app does not
need to interpret the ink.

The tablet interface (layout, controls, style) is in
[specs/tablet-ui.md](specs/tablet-ui.md).

## Base: keep from Write

These define the app. New features must not break them.

- Native format is a directory of standalone SVG pages ([FORMAT.md](FORMAT.md)).
- Discrete fixed-size pages, one printed sheet each; A4 by default, size set
  per notebook. No infinite canvas.
- The pen draws; fingers pan and zoom.
- Sync conflict copies of pages are detected and resolved side by side
  ([FORMAT.md](FORMAT.md)).
- Handwriting-aware reflow: line, word, and column structure of ink.
- Insert horizontal and vertical space into existing ink.
- Ruled erase and ruled select.
- Lasso select, move, resize of ink.
- Bookmarks placed in the ink, and links inside and between documents.
- Clippings library.
- Split view of two documents.
- SVG page backgrounds (paper color, lined, grid, dotted) and templates.
- Configurable pens, built on google/ink brushes.
- Unlimited undo and redo.
- PDF export.
- Folders on the filesystem are the library. Sync is the filesystem's job (iCloud Drive,
  Dropbox, Nextcloud, git through Files). The free Apple Account cannot use the
  iCloud entitlement, so the app must not need it.

## Features to add, in priority order

| # | Feature | Requirement | Mechanism |
| --- | --- | --- | --- |
| 1 | PDF annotation | Open a PDF from Files or the share sheet. Import renders each PDF page to a PNG 4128 px wide (2× the 2064 px portrait width of a 13-inch iPad Pro); that image is the page background, and ink goes on top. Each page keeps the size of its PDF page. After import the app does not use the PDF. Blank pages can be inserted between imported pages. Export writes the pages, background images and ink, to a new PDF. | Host rasterizer at import (web: Artifex's `mupdf` WASM package in a Web Worker; iPad: PDFKit); Skia PDF backend at export; share sheet and file picker in the hosts |
| 2 | User-visible layers | Create, name, hide, show, reorder, and lock layers per notebook. The layer list is in `notebook.json`; each page stores one SVG `<g>` per layer. Export can include or exclude each layer. | SVG groups ([FORMAT.md](FORMAT.md)) |
| 3 | Shape recognition | Hold the pen still for 300 ms at the end of a stroke to snap it to a line, circle, ellipse, rectangle, triangle, or arrow. Rectangles and ellipses may be rotated. An arrow is a shaft and a head drawn as recent strokes; the hold on the last stroke recognizes them together. A preview shows during the hold; moving the pen before lift scales and rotates the shape. The result is an SVG shape element with the pen's color and width. | Xournal's inertia recognizer and recent-stroke queue; Halíř–Flusser ellipse fit; mobile-ink's hold detection |

## Out of scope

Managed cloud sync, account libraries, AI summarization and chat, flashcards,
sticker and template stores, real-time collaboration.
