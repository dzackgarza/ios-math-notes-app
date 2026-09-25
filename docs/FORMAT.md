# Storage and file format

The authoritative state is an ordinary directory tree of documented,
standard files at a location the user chooses. The app has no private library
and no sync of its own. Saving writes the file in place; whatever owns the
directory (Dropbox, iCloud Drive, Nextcloud, git, rsync) moves it from there.

## Invariants

1. Every page is a valid standalone SVG file, and opening it in a browser
   with no app code shows the ink.
2. Every other object is a file in a documented standard format (PNG,
   JPEG, SVG, JSON).
3. The notebook is fully rebuilt from its directory. Caches (thumbnails,
   render caches) are disposable, and deleting them loses nothing.
4. Saves edit files in place at their existing paths. No import or export
   step stands between the user and the files.

## Layout

The folder hierarchy is the organization. The app discovers it on every
scan; a notebook moved or renamed by any other tool is simply found at its
new path.

```text
Notes/                         root the user picked
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
  "pages": [
    { "id": "c718…", "file": "pages/0001.svg" },
    { "id": "9a02…", "file": "pages/0002.svg" }
  ]
}
```

One file per page, so a stroke on page 217 changes only `pages/0217.svg`:
sync moves one small file, and two devices editing different pages do not
conflict. Assets are separate files, not base64 inside SVG.

## Page SVG

- Only core SVG elements: `path`, `g`, `image`, `use`, `clipPath`,
  transforms. No `foreignObject`.
- A stroke is a filled `path` whose `d` is the brush outline, so any SVG
  renderer draws the variable-width ink correctly. The app's model data
  (input samples, pen, timestamps) goes in namespaced attributes or
  `<metadata>`, which SVG 2 requires renderers to keep and ignore.
- Stable IDs on every page, layer (`g`), and stroke. Reordering or renaming
  never changes identity.
- Deterministic serialization: fixed element and attribute order, fixed
  number formatting, no whitespace churn, no generated thumbnails in the
  file. Diffs, git, and sync history stay readable.
- An imported PDF page is an `<image>` of its PNG in `assets/`, below the
  ink layers.

Plain `.svg` only; `.svgz` is not written. ZIP is only a transport form of
a notebook directory (send, archive, download).

## Access per host

| Host | Access to the notebook root |
| --- | --- |
| iPad | Files folder picker (`UIDocumentPickerViewController` for folders). Any File Provider works: On My iPad, iCloud Drive, Dropbox, others. The app keeps a security-scoped bookmark for later launches; the grant covers the tree. Pages save through `UIDocument` / `NSFileCoordinator` for coordinated, atomic writes and external-change handling. |
| Web, Chromium | `showDirectoryPicker({mode: "readwrite"})`; the directory handle is kept in IndexedDB and re-permitted on launch. |
| Web, Firefox and Safari | A WebDAV server on localhost that serves the root, for example `rclone serve webdav ~/Notes --addr 127.0.0.1:31415`. The app reads and writes through WebDAV; it stays a web app. WebDAV has no change events, so the app rescans on focus. |
| Web, read-only | A notebook directory on any static web server opens by URL of its `notebook.json`. |

OPFS and IndexedDB hold only caches, never notes.

## Write documents

Write's single-file `.svg`/`.svgz` documents import into a notebook
directory. The app does not write that format.
