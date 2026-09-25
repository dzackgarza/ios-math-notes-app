# Storage and file format

The authoritative state is an ordinary directory tree of documented,
standard files at a location the user chooses. The app has no private library
and no sync of its own. Saving writes the file in place; whatever owns the
directory (Dropbox, iCloud Drive, Nextcloud, git, rsync) moves it from there.

## Invariants

1. Every page is a valid standalone SVG file, and opening it in a browser
   with no app code shows the ink and the background.
2. Every other object is a file in a documented standard format (PNG,
   JPEG, SVG, JSON, InkML inside SVG metadata).
3. The notebook is fully rebuilt from its directory. Caches (thumbnails,
   render caches) are disposable, and deleting them loses nothing.
4. Saves edit files in place at their existing paths. No import or export
   step stands between the user and the files.
5. Saving the same document twice gives the same bytes, on every host.

## Layout

The folder hierarchy is the organization. The app discovers it on every
scan; a notebook moved or renamed by any other tool is simply found at its
new path. A directory is a notebook when it holds a `notebook.json`.

```text
Notes/                         root the user picked
├── .pens.json                 pen presets, shared by all devices
├── .templates/                page templates (notebook directories)
├── .clippings/                clippings library (a notebook directory)
├── .trash/                    notebooks deleted in the app
└── Algebraic Geometry/
    └── stable-pairs/          one notebook = one directory
        ├── notebook.json
        ├── pages/
        │   ├── 0001.svg
        │   └── 0002.svg
        └── assets/
            ├── p0017.png
            └── diagram.png
```

`notebook.json`:

```json
{
  "format": "math-notes",
  "version": 1,
  "title": "Stable pairs",
  "pageSize": "A4",
  "template": "ruled-medium",
  "layers": [
    { "id": "l-8f3kq0", "name": "Ink", "hidden": false, "locked": false }
  ],
  "pages": [
    { "id": "p-c718xa", "file": "pages/0001.svg" },
    { "id": "p-9a02mm", "file": "pages/0002.svg" }
  ]
}
```

- `pageSize` is `"A4"`, `"Letter"`, or `[width, height]` in points. It is the
  size of new pages. Each page SVG carries its own size.
- `layers` is the notebook's layer list, bottom to top. A notebook has at
  least one layer.
- `pages` gives the page order. Moving a page reorders this array; files
  are never renamed.
- Keys are written in the order shown, two-space indent, one trailing
  newline.

One file per page, so a stroke on page 217 changes only `pages/0217.svg`:
sync moves one small file, and two devices editing different pages do not
conflict. Assets are separate files, not base64 inside SVG.

### Page files

- A new page file gets the next unused four-digit number in `pages/`
  (after `0009.svg` comes `0010.svg`, whatever its position). Its name never
  changes after creation.
- A file in `pages/` that `notebook.json` does not list is shown after the
  listed pages and marked as unlisted. The app never deletes it on its own.
- A file in `pages/` that does not parse as SVG (for example, one with git
  merge-conflict markers) is shown as an error page with the parser message.
  The app never writes over it; the user fixes or deletes it outside the
  app.

## Page SVG

```xml
<svg xmlns="http://www.w3.org/2000/svg"
     xmlns:mn="https://github.com/dzackgarza/math-notes-app/ns/1"
     xmlns:inkml="http://www.w3.org/2003/InkML"
     id="p-c718xa" width="210mm" height="297mm" viewBox="0 0 595.28 841.89">
  <metadata>
    <inkml:traceFormat xml:id="xytfa">…</inkml:traceFormat>
  </metadata>
  <g id="background" mn:ruling="lined" mn:y-ruling="19.2"
     mn:y-offset="57.6" mn:x-ruling="0" mn:margin-left="48">
    <rect width="595.28" height="841.89" fill="#FFFFFF"/>
    <path d="…" fill="none" stroke="#9F9FFF" stroke-width="0.5"/>
  </g>
  <g id="l-8f3kq0">
    <path id="s-3kd92lq0mzpa" transform="translate(12.5,-4)"
          fill="#1A1A1A" mn:brush="pressure-pen" mn:brush-version="1"
          mn:size="1.6" mn:time="2026-09-25T17:43:21.123Z"
          d="M104.2 51.37l0.4 -0.12…Z">
      <metadata><inkml:trace contextRef="#xytfa">…</inkml:trace></metadata>
    </path>
  </g>
</svg>
```

- Only core SVG elements: `path`, `g`, `image`, `a`, `rect`, `line`,
  `polygon`, `ellipse`, `circle`, `metadata`, transforms. No
  `foreignObject`, no `<style>`, no `<use>` across files.
- Units: the `viewBox` is in PostScript points (1/72 inch), the unit of PDF
  pages and of PDF export. `width` and `height` are in `mm`, so the file
  prints at its physical size. A4 is 595.28 × 841.89 pt.
- Element order: `metadata`, then `g#background`, then one `g` per layer in
  `notebook.json` order, with the layer id as its `id`.
- A stroke is a filled `path` whose `d` is the brush outline: every outline
  of the stroke as one `M…Z` subpath, `fill-rule` nonzero (the default). Any
  SVG renderer draws the variable-width ink correctly.
- The stroke's input samples are an [InkML](https://www.w3.org/TR/InkML/)
  `trace` in the path's `metadata`. The page's root `metadata` declares one
  `inkml:traceFormat` per channel set that its strokes use. Channels, in
  this order when present: `X`, `Y` (pt), `T` (ms from `mn:time`), `F`
  (force 0..1), `OE` (altitude, rad), `OA` (azimuth, rad), `OR` (roll, rad).
  Values after the first point use InkML first-difference encoding (`'`).
- Stroke attributes, in this order: `id`, `class` (only for shape elements),
  `transform`, `fill`, `fill-opacity`, `mn:brush`, `mn:brush-version`,
  `mn:size`, `mn:time`, `d`. `mn:brush` names a google/ink stock brush
  family and `mn:brush-version` its version enum, so a stored stroke
  regenerates the same way after a library upgrade. `mn:time` is the UTC
  start time of the stroke.
- `d` and the samples are in stroke-local coordinates. Moving, resizing, or
  rotating a stroke writes only its `transform` attribute
  (`translate(x,y)` or `matrix(a,b,c,d,e,f)`).
- The engine regenerates a stroke's outline only when the stroke is created
  or its samples or brush change. A loaded outline is written back as it was
  read.
- IDs: `p-` pages, `l-` layers, `s-` strokes and shape elements, `b-`
  bookmarks. Each is the prefix plus 6 (pages, layers) or 12 (strokes,
  bookmarks) characters of lowercase base32 from a seedable generator.
  Reordering or renaming never changes an id. A pasted element whose id
  already exists on the page gets a new id.
- Numbers: coordinates, sizes and transforms with 2 decimals; force and
  angles with 3 decimals; `T` in whole milliseconds. Written with
  `std::to_chars` fixed format, `-0` written as `0`, trailing zeros removed.
  `d` is an absolute `M` followed by relative `l` commands. Colors are
  `#RRGGBB` in upper case; opacity goes in `fill-opacity`.
- XML is written one element per line, two-space indent, attributes in the
  orders above. No generated thumbnails in the file. Diffs, git, and sync
  history stay readable.

### Background

- `g#background` holds the page's paper: a filled `rect` and the ruling
  lines, drawn as ordinary SVG so any viewer shows them.
- `mn:ruling` is `blank`, `lined`, `grid`, or `dotted`. `mn:y-ruling` (line
  spacing), `mn:y-offset` (y of the first line), `mn:x-ruling` (grid
  spacing, 0 when none) and `mn:margin-left` are in points. Ruled select,
  ruled erase, reflow, and insert space read their line positions from
  these attributes. A `blank` page uses a line spacing of 28.8 pt, anchored
  at the pen-down point (Write's `blankYRuling`).
- An imported PDF page is an `<image>` of its PNG in `assets/`, first
  inside `g#background`.
- A new page copies the background of the notebook's `template`.

### Links and bookmarks

- A bookmark is a `g` with an id `b-…` and `class="mn-bookmark"` around the
  bookmarked ink, or around a flag path drawn in the left margin.
- A link is an SVG `a` element around the linked ink. Its `href` is a URL,
  or a path relative to the page file plus a fragment:
  `0003.svg#b-2kq9…` for a page of the same notebook,
  `../../MMP/flips/pages/0001.svg#b-…` for another notebook. The link then
  works in a browser that opens the page file, and keeps working when the
  whole tree moves.

### Shape elements

A recognized shape (FEATURES.md) is a `line`, `polygon`, `rect`, `ellipse`,
or `path` (arrow) with `class="mn-shape"`, `fill="none"`, and the pen's
color and size as `stroke` and `stroke-width`.

Plain `.svg` only; `.svgz` is not written. ZIP is only a transport form of
a notebook directory (send, archive, download).

## Other files

- `Notes/.pens.json`: the pen presets, an array of
  `{ "id", "name", "brush", "brushVersion", "color", "size" }`, in toolbar
  order.
- `Notes/.templates/<name>/`: a notebook directory. Page 1's background is
  the template. The app creates `blank`, `lined` (wide, medium, narrow:
  y-ruling 21.6, 19.2, 16.8 pt; margin 48 pt), `grid` (16.8, 14.4, 9.6 pt)
  and `dotted` templates on first use, from Write's ruling presets.
- `Notes/.clippings/`: a notebook directory; each page is one clipping,
  sized to its content.

## Access per host

| Host | Access to the notebook root |
| --- | --- |
| iPad | Files folder picker (`UIDocumentPickerViewController` for folders). A File Provider works when it supports folder picking; Dropbox has since May 2024. The app keeps a `.minimalBookmark` bookmark, created while access is started, and refreshes it when stale. Reads and writes go through `NSFileCoordinator`; a page is written with `Data.write(options: .atomic)`. One `NSFilePresenter` on the root reports external changes. |
| Web, Chromium | `showDirectoryPicker({mode: "readwrite"})`; the directory handle is kept in IndexedDB and re-permitted on launch through a "Reconnect folder" button. Writes use `createWritable({mode: "exclusive"})`. Scans skip `*.crswap` files. |
| Web, Firefox and Safari | A WebDAV server on localhost that serves the root: `rclone serve webdav ~/Notes --addr 127.0.0.1:31415 --allow-origin http://localhost --dir-cache-time 0s`. A write is a PUT to a temporary name in the same directory, then a MOVE over the target. WebDAV has no change events, so the app rescans on focus. |
| Web, read-only | A notebook directory on any static web server opens by URL of its `notebook.json`. |

OPFS and IndexedDB hold only caches, never notes.

## Sync conflicts

The app detects conflicts in three ways, and never merges silently.

1. **A conflict copy by file name**, for a page or for `notebook.json`:

   | Provider | Name of the copy of `0007.svg` |
   | --- | --- |
   | Dropbox | `0007 (<user>'s conflicted copy YYYY-MM-DD).svg` |
   | Nextcloud | `0007 (conflicted copy [<user> ]YYYY-MM-DD HHMMSS).svg`, or `0007 (case clash from …).svg` |
   | Syncthing | `0007.sync-conflict-YYYYMMDD-HHMMSS-<7-char device id>.svg` |
   | iCloud Drive (two devices create the same new file) | `0007 2.svg` |
   | OneDrive | `0007-<device name>.svg` |
   | Google Drive for desktop | `0007 (1).svg` |

   Independent of the provider: a file in `pages/` that `notebook.json` does
   not list, and whose name starts with the base name of a listed page, is a
   conflict copy of that page. The table only labels the provider.
2. **An iCloud edit conflict** creates no file: iCloud keeps the other
   versions as `NSFileVersion`s. The iPad host checks
   `NSFileVersion.unresolvedConflictVersionsOfItem(at:)` for every page and
   for `notebook.json` it loads.
3. **A page that does not parse** (see Page files).

A conflicted notebook is marked in the library. The resolution view shows
the two versions side by side with linked scrolling and zoom, and offers:
keep one, keep both as separate pages, or copy strokes from one into the
other. For `notebook.json` it shows the two page orders and layer lists and
the user keeps one; pages listed in neither become unlisted pages. Resolving
deletes the losing file (or marks the `NSFileVersion` resolved).

## Write comparison corpus

The user's Write notes are in `~/Downloads/Original Notes.zip` (ten
`write-v3` `.svgz` documents). They serve only to compare behavior against
Write by hand, and stay out of this public repository. The fixtures in
`tests/fixtures/write/` are recorded from synthetic input.

## Conventions

- Undo history is kept for the open session only.
- Pens, templates, and clippings live under the notes root, so every device
  that opens the root sees the same ones.
