# TikZ drawing mode

> Product contract for [Drawing mode issue #10](https://github.com/dzackgarza/math-notes-app/issues/10).
> The issue tree owns execution. The project agent-memory plan
> `PLAN-TIKZ-DRAWING-MODE` points here.

## Result

The user turns on Drawing mode while editing a note, draws on a page, and
turns the mode off to complete the figure. The app groups the ink made during
that interval into one selectable figure. A bounding rectangle encloses its
ink. The figure retains the strokes, an editable geometric scene, and editable
TikZ source. The user can inspect the resulting figure and source in a preview
sidebar, revise either view, and reopen the figure later. The page remains a
standalone SVG that shows the figure without the app.

The drawing editor is a fork of
[FreeTikZ](https://github.com/chrisheunen/freetikz), integrated with Math Notes.
FreeTikZ's pen-first capture is the starting interaction. General mathematical
figures use standard TikZ and only the libraries they need. The custom
`freetikz.sty` remains relevant to its specialized string-diagram vocabulary.

## Mode and page interaction

1. Turning Drawing mode on starts one capture session on the current page.
   Subsequent pen strokes enter that session. Ordinary note ink remains outside
   it. The user can select, move, and edit captured strokes without ending the
   session. Page navigation or closing the note must resolve the active session
   explicitly; it must not lose ink or silently complete the figure.
2. The app preserves every original stroke and its input samples. Recognition
   adds an interpretation; it never destroys the ink. The user can accept,
   reject, or change an interpretation.
3. Turning Drawing mode off completes the current session. The app computes
   the union of the captured strokes' page-space bounds and places one visible
   bounding rectangle around them. The figure behaves as one selectable page
   object for move, resize, copy, and delete. Reopening it restores the scene,
   source, and original ink for further editing. An empty session creates no
   figure.
4. The rectangle identifies the work captured in that session. It is an edit
   boundary, not a page background or an image export. If a figure is moved or
   resized, its bounds and contents change together. Two completed sessions
   remain two figures until the user explicitly groups them.
5. The preview sidebar shows the interpreted figure and the current TikZ
   source. It identifies recognition choices and errors. Canvas selection
   selects the corresponding source range; source selection identifies the
   corresponding canvas object. Fast preview may approximate TeX, but any
   final rendered preview used to judge labels or layout uses the project
   preamble and TeX engine.

## Figure representation and ownership

The persistent representation separates three kinds of information:

| Part | Owns |
| --- | --- |
| Original ink | Pen samples and visible stroke outlines, unchanged by recognition. |
| Geometric scene | Objects, styles, control points, constraints, relations, groups, layers, and stable IDs. |
| TikZ source | Authored text, object-to-source ranges, and opaque commands the visual editor cannot interpret. |

The geometric scene starts with `RawStroke` and supports `Point`, `Line`,
`Ray`, `Segment`, `Circle`, `Ellipse`, `Arc`, `BezierCurve`, `Spline`,
`ClosedRegion`, `Node`, `Arrow`, `Text`, `MathLabel`, and `Group`. Relations
include coincidence, point-on-curve, parallelism, perpendicularity, tangency,
equal length or radius, alignment, equal spacing, symmetry, intersection, and
attachment. A domain-specific layer may add mathematical objects and lower
them to this geometric scene; it must not replace the general scene.

The scene is the editable interpretation of the figure, not a coordinate trace.
The source is authoritative wherever the user has edited it. A visual edit
changes only the syntax owned by the edited object or property. Unrecognized
TikZ remains intact as opaque source. A parser must retain source ranges,
comments, spacing, and unknown commands. A source edit updates the visual
object when its syntax is understood; otherwise it stays visible as authored
source and reports the visual limitation. Saving never replaces authored TikZ
with regenerated output.

Math Notes stores the figure with the notebook files. The page SVG contains a
standalone visible representation and a stable figure reference. The editable
scene, ink, and `.tikz` source are documented files in the notebook, not a
private app database or a raster-only image. A figure copied to another note
brings those files and receives new object IDs. The exact on-disk schema is a
child-plan decision that must preserve the invariants of
[FORMAT.md](../FORMAT.md): independent SVG pages, in-place saves, rebuild from
the folder, and deterministic bytes across hosts.

## Recognition and geometry

The processing path is:

`ink → geometric primitives → constraints and relations → mathematical objects → TikZ`

Freehand recognition is one way to create or revise scene objects. Direct
creation and selection work without recognition. Initial recognition reduces
strokes to salient endpoints, extrema, inflections, intersections, and pinned
points. The user can retain raw ink, accept a suggested primitive, or select a
different one. Sophisticated automatic classification comes after the editable
scene and source workflow.

Beautification uses relations before independent coordinate snapping. It can
infer a common coordinate frame, exact parallelism and perpendicularity,
midpoints, rectangles, symmetry, and equal spacing. Approximate coordinates
may become small integers or rationals and common angles such as 30, 45, 60,
and 90 degrees. A relation the user accepts must remain exact when the figure
changes. Snapping controls cover grid, points, anchors, midpoints,
intersections, tangencies, horizontal and vertical directions, angle steps,
baselines, and equal spacing. Constraints are inspectable and removable.

## Editing surface

The drawing editor provides select and multiselect, freehand, node/path,
line/polyline, Bézier/spline, circle/ellipse/arc, polygon/region, and TeX-label
tools. Selection exposes handles and numeric position, dimension, radius,
angle, and control-point fields. Bézier nodes support cusp, smooth, and
symmetric modes. The user can group, set layers and z-order, duplicate,
rotate, reflect, align, and distribute objects. Constraints can be created
from a selection, such as two parallel segments or a point on a curve.

A label stores arbitrary project TeX, including macros. The figure reads the
project preamble or a designated figure preamble. Label properties include
anchor, side, offset, rotation, sloping, swap, alignment, text width, and
background. An edge label belongs to its edge. Browser rendering may be fast
and approximate; final label bounds come from the TeX engine.

## TikZ output

Generated source expresses recognized structure. Named nodes and relative
positioning express layouts; `calc` expresses derived coordinates;
`intersections` expresses intersection points. Repeated styles and spacing
become named definitions when this shortens and clarifies the source. The
generator prefers exact relations, small rational values, standard
constructions, and readable names over unrelated decimal coordinates. It
keeps a plain-TikZ representation available when a specialized backend does
not apply.

Smooth hand-drawn curves are reduced to salient points. The default semantic
backend may emit Hobby splines; the user can convert a curve to explicit
Bézier controls for precision editing. Backend adapters may target TikZ
`positioning`, `calc`, `intersections`, `arrows.meta`,
`decorations.markings`, Hobby, `spath3`, `braids`, `tikz-cd`, graph drawing,
`forest`, `tkz-euclide`, `pgfplots`, `dynkin-diagrams`, and `tikz-3dplot`.
Each adapter declares the required package or library. The plain-TikZ path
continues to work when an adapter is absent. A specialized backend cannot
erase scene semantics or authored source.

Diagram vocabularies may add topology, algebraic geometry, toric and lattice,
or categorical objects. They are modes of one editor, with shared scene and
source rules. Contextual confirmation of a crossing, singularity, branch
point, morphism, or other semantic object can precede automatic recognition.

## Delivery order and acceptance

| Stage | Result and proof |
| --- | --- |
| 1. Scene and capture | Toggle Drawing mode, draw, complete, save, reload, and reopen a figure. The same strokes, samples, IDs, scene objects, bounds, and page SVG survive. Selection and geometric editing work. |
| 2. Source round-trip | Edit a supported TikZ property and see the canvas change; edit the canvas and see only its owned source span change. Comments, whitespace, and unknown commands survive save and reload. Source and canvas selection correspond. |
| 3. Labels | A figure with custom project macros renders with the project preamble. Label moves remain attached to their semantic object; final bounds agree with TeX output. |
| 4. Relations and generation | Accepted constraints remain exact after edits. Generated source uses relative nodes, derived points, and shared styles where those relations exist. |
| 5. Precision editing | Curves, layers, alignment, style controls, and numeric edits survive save and reopen. The sidebar shows the compiled figure and diagnostics. |
| 6. Semantic backends | Each added backend proves a representative semantic figure can be edited through both canvas and source, compiled with its declared libraries, and kept in the note. |
| 7. Recognition | Representative pen sketches offer correct, reversible interpretations without changing the stored ink. |

The first usable research-note workflow is draw, label, inspect, complete,
reopen, and copy TikZ. Publication refinement uses the same figure: constrain,
align, edit controls and source, and compile with the paper preamble. There is
no export-and-reimport step between those workflows.

## Existing feature tree

The root is [#11](https://github.com/dzackgarza/math-notes-app/issues/11).
Drawing work belongs under feature additions
[#17](https://github.com/dzackgarza/math-notes-app/issues/17). The active
editing baseline [#56](https://github.com/dzackgarza/math-notes-app/issues/56)
remains a prerequisite to treating the web app as usable.

| Existing issue or contract | Disposition |
| --- | --- |
| [#10](https://github.com/dzackgarza/math-notes-app/issues/10) drawing mode | Tracks the bounded figure and editor workstream. |
| [#35](https://github.com/dzackgarza/math-notes-app/issues/35) iPad Pencil interactions | Keeps the Write-parity tools and Pencil gestures. Figure-completion feedback applies to the drawing-mode interaction. |
| [#9](https://github.com/dzackgarza/math-notes-app/issues/9) layers | Keep notebook layers. Figure-internal layers are a separate scene concern; define their interaction in the format child plan. |
| [#29](https://github.com/dzackgarza/math-notes-app/issues/29) PDF export | Keep. Export must paint the page-visible figure. |
| [#33](https://github.com/dzackgarza/math-notes-app/issues/33) clippings | Keep. A completed figure can be selected and clipped without flattening its edit state. |
| [#60](https://github.com/dzackgarza/math-notes-app/issues/60) image tool | Keep. A general image is not a TikZ figure. |
| [#61](https://github.com/dzackgarza/math-notes-app/issues/61) text tool | Keep. General page text is distinct from a TeX label owned by a figure. |
| [#49](https://github.com/dzackgarza/math-notes-app/issues/49), [#58](https://github.com/dzackgarza/math-notes-app/issues/58), [#62](https://github.com/dzackgarza/math-notes-app/issues/62), [#63](https://github.com/dzackgarza/math-notes-app/issues/63) | Keep their notebook, note, tab, and draft behavior. |
| [#8](https://github.com/dzackgarza/math-notes-app/issues/8) PDF annotation | Keep. Its page backgrounds and imported page sizes are independent of figures. |

Issue #10 groups independently trackable scene/capture, source round-trip,
label/preamble, constraints/generation, precision editing, and backend work.
Issue nodes are tracking units, not prescribed PR boundaries. The first
coherent implementation milestone should cover the complete mode-to-embedded-
figure workflow before a PR is opened.

## Decisions still needed at the child-plan boundary

- The exact page-to-figure file reference and file schema, including how a
  standalone SVG embeds a useful visible figure without losing the ink.
- Whether completion draws a persistent visible border or a selection outline
  that appears only while the figure is selected. The bounding rectangle and
  its selection behavior are required either way.
- The source parser and TeX compilation integration, based on a survey of
  existing TikZ editor and parser implementations.
- The interface for sharing the drawing editor between the web and native
  iPad hosts without moving the iPad canvas into a web view.
