# Ink reflow: dependency assessment

Decision owners: [#30](https://github.com/dzackgarza/math-notes-app/issues/30)
and [#31](https://github.com/dzackgarza/math-notes-app/issues/31).
Policy: [component ownership](ARCHITECTURE.md#component-ownership).
Survey date: 2026-09-27.

## Finding

Handwritten-ink reflow has an established SDK implementation. MyScript iink
documents reflow of handwriting as well as typeset content. Apple PencilKit
supplies smart selection and insert-space. These are ink-editor capabilities;
their use in mathematical notes does not establish a need for local algorithms.
[MyScript responsive layout](https://developer.myscript.com/docs/interactive-ink/4.4/concepts/responsive-layout/),
[Apple PencilKit](https://developer.apple.com/videos/play/wwdc2020/10107/).

The product needs to retain original editable ink while moving words and
lines, respect mixed mathematical content, and carry overflow onto fixed-size
pages. Its durable note is standalone SVG with stored samples and authored
TikZ. The assessment must distinguish these data requirements from choices in
the current C++ engine.

## Candidates

| Candidate | Evidence from primary sources | Fit to establish |
| --- | --- | --- |
| MyScript iink | [Responsive layout](https://developer.myscript.com/docs/interactive-ink/4.4/concepts/responsive-layout/) includes handwritten reflow. [Content types](https://developer.myscript.com/doc/interactive-ink/4.5/overview/content-types/) include text, math, and structured documents. [JIIX](https://developer.myscript.com/doc/interactive-ink/4.5/reference/jiix/) carries stroke samples, identity, words, and line structure. | Exact ruled edits and fixed-page overflow; preservation of source samples and figures; SVG import/save mapping. The [platform matrix](https://developer.myscript.com/doc/interactive-ink/4.5/overview/platforms/) directs offline use to native SDKs and documents web feature limits. Verify deployment and distribution terms before choosing it. |
| Apple PencilKit | [Apple's WWDC20 session](https://developer.apple.com/videos/play/wwdc2020/10107/) documents built-in smart selection and insert-space in PKCanvasView. [WWDC19](https://developer.apple.com/videos/play/wwdc2019/221/) describes its UIScrollView integration. | Compare complete native editor adoption with the Metal adapter. Verify the required word/line and cross-page edit, source fidelity, and a web-host counterpart. Insert-space alone does not establish all reflow requirements. |
| Wacom WILL | [The product documentation](https://developer.wacom.com/zh-cn/products/will-sdk-for-ink) describes cross-platform ink rendering, storage, stroke transforms, and precise erasure. | The inspected product page does not establish word/line reflow. Obtain the supported API scope and integration terms before deciding whether its editing stack can own this behavior. |
| Microsoft InkAnalyzer | [InkAnalyzer](https://learn.microsoft.com/en-us/uwp/api/windows.ui.input.inking.analysis.inkanalyzer) and [analysis node kinds](https://learn.microsoft.com/en-us/uwp/api/windows.ui.input.inking.analysis.inkanalysisnodekind) expose handwriting structure, including writing regions, paragraphs, lines, and words. | The inspected API establishes analysis on Windows, not an original-stroke reflow operation for the web and iPad hosts. Evaluate it as prior art and assess any portable supported owner separately. |
| google/ink | [The module documentation](https://github.com/google/ink/blob/main/README.md) covers stroke construction, geometry, rendering, and storage. | Document reflow is not established by the inspected module contract. Existing use of this library does not assign all remaining ink behavior to Math Notes. |
| Stylus Labs Write | [Selection::reflowStrokes](https://github.com/styluslabs/Write/blob/401b65d/syncscribble/selection.cpp) operates on original ink elements using line and word geometry. The [repository](https://github.com/styluslabs/Write) builds a complete application. | Evaluate maintained fork/component integration and its Page, Element, and selection dependencies. A source port is an ownership exception that needs the same necessity evidence as new code. |

## Decision still required

Evaluate MyScript's complete editor and PencilKit's native editor before
restricting the choice to small geometry libraries. Compare a maintained Write
integration with those options. A larger dependency may remove several local
subsystems; include that effect in the comparison.

For each candidate, record a supported version, maintenance and license
evidence, and a representative notebook result. Check that importing, editing,
undoing, saving, and reopening retain stroke identity and samples, mathematical
figures, and their authored source. Check both host and offline requirements.
Treat a missing evaluation as unresolved, not as evidence of incompatibility.

Select the owner and its supported extension boundary in #30/#31. If a
required operation remains uncovered, identify that exact gap and explain why
Math Notes must own it. Any new core responsibility requires the architecture
decision and approval specified by the ownership policy. This survey does not
authorize a custom reflow engine.

## Search record

Sources searched: official MyScript, Apple, Wacom, and Microsoft documentation;
the google/ink and Stylus Labs Write repositories; upstream product and API
examples. Direct source inspection included Write's `reflowStrokes`. The links
above identify the primary material used in the findings. This is a published
capability assessment; integration behavior remains to be verified.

The following are exact queries used to locate and compare those owners:

```text
MyScript iink SDK handwriting reflow handwritten text block resize raw content
site.developer.myscript.com reflow iink SDK
site.github.com/styluslabs/Write reflowStrokes
site:developer.myscript.com iink SDK reflow handwritten ink text blocks reflow original strokes
site:developer.myscript.com iink SDK text document reflow export original strokes handwriting
site:developer.wacom.com WILL ink SDK semantic reflow handwriting layout
site:learn.microsoft.com InkAnalyzer handwriting recognition reflow original ink strokes
site:developer.myscript.com/doc/interactive-ink reflow handwriting strokes original ink Text Document
site:developer.myscript.com "reflow" "handwriting" "strokes"
site:developer.myscript.com reflow text document handwritten words geometry
site:developer.myscript.com iink SDK supported platforms iOS web offline cloud recognition license
site:developer.myscript.com/doc/interactive-ink/4.4 "reflow" "handwriting"
site:developer.myscript.com/doc/interactive-ink/4.4 "reflow" "strokes"
site:developer.myscript.com/doc/interactive-ink/4.4 "Text" "reflow" "ink"
site:developer.myscript.com/docs/interactive-ink/latest "reflow" handwriting
site:developer.apple.com documentation PencilKit PKCanvasView PKDrawing stroke selection handwriting recognition text reflow
site:developer.wacom.com WILL SDK ink reflow text layout handwriting original strokes
site:github.com/google/ink README stroke brush geometry no document reflow
site:github.com/styluslabs/Write reflowStrokes selection.cpp AGPL embed library
site:developer.apple.com PencilKit "insert space" handwritten ink reflow
site:developer.apple.com "PKCanvasView" "insert space"
site:developer.apple.com PencilKit smart selection insert space APIs editable strokes
site:developer.wacom.com "reflow" "ink" SDK
site:developer.wacom.com/en-us/products/will-sdk-for-ink recognition reflow edit stroke layout
site:developer.wacom.com/en-us WILL SDK for ink text reflow handwriting API
site:github.com/Wacom-Developer "reflow" "Universal Ink"
site:developer.wacom.com universal ink model data model brush renderer recognition sdk
site:learn.microsoft.com InkCanvas stroke handwriting selection erase reflow ink WPF
site:learn.microsoft.com InkAnalyzer analysis layout line paragraph writing classification context nodes
site:learn.microsoft.com Windows Ink handwriting insert space reflow original strokes API
site:learn.microsoft.com InkCanvas Strokes selection moved resize gesture API
github styluslabs Write README source code AGPL reflowStrokes selection.cpp
site:github.com/styluslabs/Write "reflowStrokes"
site:github.com/styluslabs/Write README library embed build Qt SDL AGPL
site:styluslabs.com Write reflow handwritten text insert space
site:developer.myscript.com/doc/interactive-ink/4.4 supported platforms Web iOS on device license offline
site:developer.myscript.com/doc/interactive-ink/4.4 web cloud recognition server connection offline
site:developer.myscript.com/doc/interactive-ink/4.4 license commercial on device SDK developer license
site:developer.myscript.com/doc/interactive-ink/4.4 import existing SVG strokes raw ink reflow
site:learn.microsoft.com/en-us/windows/win32/tablet IInkAnalyzer layout analysis context nodes line paragraph ink
site:learn.microsoft.com/en-us/windows/apps/develop/input ink analyzer handwriting recognition text lines InkAnalysisResult
site:learn.microsoft.com/en-us/uwp/api/windows.ui.input.inking.analysis ink analyzer line words
site:learn.microsoft.com/en-us/dotnet/api/system.windows.ink InkAnalyzer line layout reflow
site:developer.myscript.com/doc/interactive-ink/4.4 "reflow" "setViewSize"
site:developer.myscript.com/doc/interactive-ink/4.4 "reflow" "Editor" "Text"
site:developer.myscript.com/doc/interactive-ink/4.4 "insert space" handwritten text ink
site:developer.myscript.com/doc/interactive-ink/4.4 "reflow" "Raw Content"
site:developer.myscript.com "handwritten or typeset" "reflow"
site:developer.myscript.com "responsive" "ink size"
```
