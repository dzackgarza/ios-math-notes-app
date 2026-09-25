# Feature spec

Math Notes is a vector handwriting app for mathematics notes and talks. The
behavioral model is [Stylus Labs Write](https://github.com/styluslabs/write):
notes are SVG files on the ordinary filesystem, and ink is reflowable. Because
the files are plain SVG, external programs can process them; the app does not
need to interpret the ink.

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
- SVG page backgrounds and templates.
- Configurable pens, built on google/ink brushes.
- Unlimited undo and redo.
- PDF export.
- Folders on the filesystem are the library. Sync is the filesystem's job (iCloud Drive,
  Dropbox, Nextcloud, git through Files). The free Apple Account cannot use the
  iCloud entitlement, so the app must not need it.

## Features to add, in priority order

| # | Feature | Requirement | Mechanism |
| --- | --- | --- | --- |
| 1 | PDF annotation | Open a PDF from Files or the share sheet. Import renders each PDF page to a PNG at 2× the iPad screen resolution; that image is the page background, and ink goes on top. After import the app does not use the PDF. Blank pages can be inserted between imported pages. Export writes the pages, background images and ink, to a new PDF. | MuPDF at import; Skia PDF backend at export; share sheet and file picker in the hosts |
| 2 | User-visible layers | Create, name, hide, show, reorder, and lock layers per document. Stored as SVG `<g>` groups. Export can include or exclude each layer. | SVG groups |
| 3 | Shape recognition | Hold the pen at the end of a stroke to snap a rough line, circle, ellipse, rectangle, triangle, or arrow to its exact shape. Result is an SVG shape element. | Stroke classifier (for example the $1/$P recognizer family) |

## Out of scope

Managed cloud sync, account libraries, AI summarization and chat, flashcards,
sticker and template stores, real-time collaboration.
