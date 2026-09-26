# Write behavior fixtures

Input traces and the results that Stylus Labs Write computes for them. The
engine tests replay the same traces and compare their results with these.

Write is the fork [dzackgarza/Write](https://github.com/dzackgarza/Write),
branch `replay-harness`, based on upstream commit `401b65d`. The justfile
variable `write_rev` pins the fork commit. `just write-fixtures` builds the
fork and regenerates every case (see "Regenerating").

## Cases

One directory per case:

| File | Content |
| --- | --- |
| `trace.txt` | The actions, one per line (grammar below). |
| `input.html` | The document before the trace, when the case needs one. Otherwise the case starts on a blank document. |
| `expected.json` | Write's result (format below). |
| `result-p<N>.svg` | Page N after the trace, written by `Page::saveSVGFile`, with the native `transform` attributes. |

| Case | Behavior |
| --- | --- |
| `upstream-test0` … `upstream-test15` | Write's own `scribbletest.cpp` tests `test0` to `test15`, converted to traces. Each replay reproduces `scribbletest/test<N>_ref.html`. |
| `reflow-column-divider` | Ruled insert space on line 1 of the left column; words reflow over lines 1 to 4; the right column, past a drawn divider, stays. |
| `ruled-erase-greedy`, `ruled-erase-nongreedy` | Ruled erase over part of a line with `greedyRuledErase` 1 and 0. The stroke that only overlaps the erased range is deleted only in the greedy case. |
| `insspace-virtual-ruling` | Ruled insert space on an unruled page. |
| `insspace-negative-erase` | Ruled insert space dragged to the left: the strokes in the swept range are deleted, and the rest of the line moves left. |
| `stroke-erase` | Stroke erase (mode 14) over five strokes drawn on a blank page: two gestures delete three strokes, one passes beside a stroke. |
| `free-erase` | Free erase (mode 16) over five strokes: one cut, two cuts in one gesture, a trimmed end, an untouched stroke, and a stroke erased whole. |

The upstream cases also cover free erase (`test11`, `test12`), stroke erase
(`test8`, `test15`), lasso select (`test6`), ruled select (`test6`, `test8`),
move and resize (`test8`, `test12`, `test14`), bookmarks (`test9`, `test12`),
reflow (`test7`, `test11`, `test15`), and undo and redo.

## Units

Write units are 1/150 inch. Multiply by 0.48 to get points.

## Replay setup

Each case starts from the setup of `ScribbleTest::runAll`: page 768 × 1024,
`yRuling` 40, `xRuling` 0, `marginLeft` 100, input smoothing off, pen black,
width 1, round tip, mode `MODE_STROKE`, zoom 1, the view at
`gotoPos(0, (0, 0))`, and a screen of 600 × 800. Every case gets a fresh
configuration, document, and view.

## Trace grammar

One command per line. Lines that start with `#` are comments. Numbers are
decimal. Colors are ARGB as unsigned 32-bit integers.

| Line | Calls |
| --- | --- |
| `ie x y p src ev mm t` | `scribbleInput->doInputEvent(sx, sy, p, src, ev, mm, t)` |
| `mode N` | `scribbleMode->setMode(N)` |
| `pen argb width flags [wRatio prParam spdMax dirAngle dash gap]` | `setPen(ScribblePen(...))` |
| `cfg key value` | `cfg->setConfigValue(key, value)` |
| `cmd N` | `scribbleDoc->doCommand(N)` |
| `mt ev1 x1 y1 ev2 x2 y2` | A touch event with one or two points (`ScribbleTest::mtinput`), in screen coordinates. `ev` 3 means no point. |
| `view page x y` | `scribbleArea->gotoPos(page, (x, y))` |
| `page N` | `scribbleArea->gotoPage(N)` |
| `screen left top width height` | Sets `scribbleArea->screenRect`. |
| `props w h xRuling yRuling marginLeft argb ruleArgb applyToAll docDefault global` | `scribbleDoc->setPageProperties(...)` |
| `hyperref url` | `createHyperRef(url)` on the selection. |
| `hyperref-bookmark y` | `createHyperRef(findBookmark(document, y))` on the selection. |
| `clearsel` | `scribbleArea->clearSelection()` |
| `recentsel` | `scribbleArea->recentStrokeSelect()` |
| `paintbookmarks` | Paints the bookmark view. `hyperref-bookmark` finds only painted bookmarks. |
| `strokeprops argb width` | `setStrokeProperties` on the selection. |
| `selmode N` | Sets `scribbleMode->moveSelMode`. |
| `pathrel 0\|1` | Sets `SvgWriter::DEFAULT_PATH_DATA_REL` (relative path data in saved SVG). |
| `clipsvg <svg fragment>` | Puts the SVG fragment on the clipboard (`importExternalDoc`). |
| `newdoc` | `scribbleDoc->newDocument()` |
| `reopen ext flags loadAll` | Saves the document as `.ext` with save flags `flags`, and opens it again. With `loadAll` 1, loads every page and deletes the saved files. |

`ie` fields:

- `x y`: page units relative to the origin of page 0, independent of pan and
  zoom. On page 0 these are page coordinates. Page N lies below page 0 at
  y offset = (sum of the heights of pages 0 to N−1) + 20·N. When pages have
  different widths, page N is centered: x offset = (width₀ − widthₙ)/2.
- `p`: pressure, 0 to 1.
- `src`: 1 mouse, 2 pen, 3 touch (`inputsource_t`).
- `ev`: 1 press, 0 move, −1 release, 2 cancel (`inputevent_t`). The
  coordinates of a release are ignored.
- `mm`: mode modifier bits; 2 is the pen button (`MODEMOD_PENBTN`).
- `t`: time in ms. Write takes each stroke's timestamp from the `t` of the
  last event before the stroke is added, so the 2.5 s window of
  `groupStrokes` is deterministic. The upstream cases use `t` = 0 for every
  event, as the upstream tests do.

A mode other than 12 lasts for one gesture (`doubleTapSticky` is off), so a
trace sets it before each gesture. Mode numbers (`scribblemode.h`): 11 pan, 12 stroke, 14 stroke erase, 15 ruled
erase, 16 free erase, 18 rectangle select, 19 ruled select, 20 lasso select,
25 vertical insert space, 27 ruled insert space, 28 bookmark, 36 page select.
Command numbers: `ID_UNDO` = 100, `ID_REDO` = 101, `ID_SELALL` = 102,
`ID_DELSEL` = 105; the rest follow the enum in `scribblemode.h`.

## expected.json

```json
{
"pages": [
 {"elements": [
  {"id": 0, "dx": 0, "dy": 40, "transform": [1, 0, 0, 1, 0, 40],
   "penPoints": [[[x, y], ...], ...],
   "children": [...]}
 ]}
],
"selected": [ids],
"deleted": [ids]
}
```

- `elements`: each page's top-level elements in document order. The position
  in this list is the element's document-order index.
- `id`: identifies an element across the trace. Elements get ids in order of
  first appearance: the elements of `input.html` in document order, then each
  new element after the trace line that creates it. An element keeps its id
  while it moves, and when undo restores it. A pasted copy and each piece of a
  free-erased stroke get new ids.
- `dx`, `dy`: the translation of the element's `transform`. `transform` is the
  full matrix `[a, b, c, d, e, f]`.
- `children`: the strokes of a multi-stroke element (a hyperlink group), with
  their transforms relative to the group.
- `penPoints`: only in cases that use free erase (mode 16). The centerline of
  each path element (`Element::toPenPoints`) in page coordinates, one list per
  subpath.
- `selected`: ids of the elements in the current selection after the trace.
- `deleted`: ids that no longer exist in the document after the trace.

## Comparison rules

- `selected` and `deleted`: exact sets.
- Translations `dx`, `dy`: within 0.01 pt.
- Pen points: within 0.5 pt.

## Regenerating

`just write-fixtures` clones the fork to `$WRITE_DIR` (default
`~/.cache/math-notes/Write`) when it is missing, requires it to be at
`write_rev`, builds it, and then:

1. Runs Write's upstream test suite (`--test`) with `WRITE_RECORD_DIR` set.
   The suite must pass; each test is also written as a trace, which becomes
   `upstream-test<N>/trace.txt`. Upstream `test<N>_in.html` files become
   `input.html`.
2. Replays every case directory (`--replaytest` with `WRITE_REPLAY_DIR`),
   writing `expected.json` and `result-p<N>.svg`. For each `upstream-test<N>`,
   the replay also saves the document as `runAll` does and fails unless the
   SVG part matches `test<N>_ref.html`.

The traces of the other cases are source files. To record a trace by hand,
run Write with `WRITE_TRACE_LOG=<file>`: every pen event is appended as an
`ie` line.
