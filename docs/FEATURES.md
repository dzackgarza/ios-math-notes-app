# Feature spec

Math Notes is a vector handwriting app for mathematics notes and talks. The
behavioral model is [Stylus Labs Write](https://github.com/styluslabs/write):
notes are SVG files on the ordinary filesystem, and ink is reflowable. Because
the files are plain SVG, external programs can process them; the app does not
need to interpret the ink.

## Base: keep from Write

These define the app. New features must not break them.

- Native format is editable vector SVG, one note per file or directory.
- Handwriting-aware reflow: line, word, and column structure of ink.
- Insert horizontal and vertical space into existing ink.
- Ruled erase and ruled select.
- Lasso select, move, resize of ink.
- Bookmarks placed in the ink, and links inside and between documents.
- Clippings library.
- Split view of two documents.
- SVG page backgrounds and templates.
- Configurable pens.
- Unlimited undo and redo.
- PDF export.
- Folders on the filesystem. Sync is the filesystem's job (iCloud Drive
  through Files, Nextcloud, git). The free Apple Account cannot use the
  iCloud entitlement, so the app must not need it.

## Features to add, in priority order

| # | Feature | Requirement | Mechanism |
| --- | --- | --- | --- |
| 1 | PDF annotation (as in Xournal++) | Open a PDF from Files or the share sheet. Its pages are page backgrounds; ink goes on top. The PDF file is never modified: the note stores a reference to it plus the annotations. Blank pages can be inserted between PDF pages. Export writes a new PDF with the original pages as vector content and the ink drawn over them. | MuPDF in the engine; share sheet and file picker in the hosts |
| 2 | User-visible layers | Create, name, hide, show, reorder, and lock layers per document. Stored as SVG `<g>` groups. Export can include or exclude each layer. | SVG groups |
| 3 | Shape recognition | Hold the pen at the end of a stroke to snap a rough line, circle, ellipse, rectangle, triangle, or arrow to its exact shape. Result is an SVG shape element. | Stroke classifier (for example the $1/$P recognizer family) |

## Out of scope

Managed cloud sync, account libraries, AI summarization and chat, flashcards,
sticker and template stores, real-time collaboration.
