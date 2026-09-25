# Feature spec

Math Notes is a vector handwriting app for mathematics on iPad. The model is
[Stylus Labs Write](https://github.com/styluslabs/write): notes are SVG files
on the ordinary filesystem, and ink is reflowable. This spec lists the features
to add on top of that model, to reach parity with Noteful, Goodnotes, and
Notability for academic work.

## Base: keep from Write

These define the app. New features must not break them.

- Native format is editable vector SVG (`.svg` / `.svgz`), one file per document.
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

| # | Feature | Requirement | iOS mechanism |
| --- | --- | --- | --- |
| 1 | PDF import and annotation | Open a PDF from Files or the share sheet. Each page becomes a document page with the PDF page as its background. Ink goes on top. Export keeps the PDF page as vector content. | PDFKit; share extension or `UIDocumentPickerViewController` |
| 2 | Editable text objects | Place a text box on a page, type, edit later, move and resize it. Stored as SVG `<text>`. Mixes with ink on the same page. | TextKit 2 |
| 3 | Handwriting recognition and search | Recognize the characters in the ink. Search all documents by handwritten text, typed text, and PDF text. Store the result so search does not re-run recognition. | Vision text recognition on rendered ink; PDFKit for PDF text |
| 4 | Document scanner | Scan paper with the camera into a new document or new pages. Scanned text is searchable by #3. | VisionKit `VNDocumentCameraViewController` |
| 5 | User-visible layers | Create, name, hide, show, reorder, and lock layers per document. Stored as SVG `<g>` groups. Export can include or exclude each layer. | SVG groups |
| 6 | Audio synchronized to ink | Record audio while writing. Each stroke stores its time in the recording. Tap a stroke to play from that time; playback shows the ink written at the playhead. | AVFoundation |
| 7 | Shape recognition | Hold the pen at the end of a stroke to snap a rough line, circle, ellipse, rectangle, triangle, or arrow to its exact shape. Result is an SVG shape element. | Stroke classifier (for example the $1/$P recognizer family) |
| 8 | Tags | `#tag` and nested `#a/b` tags on documents and on locations in a document. Browse and search by tag across all documents. | Tags stored in the SVG metadata |
| 9 | Handwritten math to LaTeX | Select handwritten math and convert it to LaTeX source and to typeset math on the page. | Needs an on-device math recognition model; Apple has none |
| 10 | Audio transcription | Transcribe recordings from #6 to text that #3 searches. | Speech framework, on device |

Features 1–5 close most of the practical gap with Noteful.

## Out of scope

Managed cloud sync, account libraries, AI summarization and chat, flashcards,
sticker and template stores, real-time collaboration.
