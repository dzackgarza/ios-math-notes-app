// An editing session on one document: pen input to live and committed
// strokes, and the history of document values (docs/ARCHITECTURE.md: undo
// and redo move an index in a list of document values).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "document/document.h"
#include "editor/erase.h"
#include "editor/history.h"
#include "ink.h"
#include "ink/brush/brush.h"
#include "ink/geometry/envelope.h"
#include "ink/strokes/in_progress_stroke.h"
#include "ink/strokes/input/stroke_input_batch.h"

namespace ink_engine {

struct Pen {
  InkBrush brush = INK_BRUSH_PRESSURE_PEN;
  Rgb color{26, 26, 26};
  float size = 1.6f;  // pt
};

// "pressure-pen", "marker", "highlighter": the family names in mn:brush.
const char *BrushName(InkBrush brush);
// The brush of a family name; an unknown name is the pressure pen.
InkBrush BrushFromName(std::string_view name);

// The eraser radius in view units: Write ERASESTROKE_RADIUS and
// ERASEFREE_RADIUS (syncscribble/scribblearea.cpp:11-12, styluslabs/Write
// 401b65d), fixed on screen so that zooming changes how much is erased.
inline constexpr double kEraserRadius = 7;

// A view point in content coordinates, through the inverse of `view`.
Point ToContent(const Transform &view, double x, double y);

// google/ink stock brush for a pen, epsilon 0.01 pt.
ink::Brush MakeBrush(const Pen &pen);

class Editor {
 public:
  explicit Editor(DocumentHistory &history) : history_(&history) {}

  // content -> view affine transform, SVG matrix order. Content coordinates
  // are those of the page layout (layout/layout.h).
  void SetView(const Transform &content_to_view) { view_ = content_to_view; }
  void SetPen(const Pen &pen) { pen_ = pen; }
  // Added to host sample times (ms) to get UTC ms since the Unix epoch.
  void SetUtcOffset(double utc_minus_host_ms) { utc_offset_ms_ = utc_minus_host_ms; }
  // The eraser of the pen's eraser end and of the eraser tool; `active`: pen
  // and mouse input erase.
  void SetEraser(InkEraser kind, bool active) {
    eraser_kind_ = kind;
    eraser_active_ = active;
  }

  void Input(const InkPenSample *samples, size_t count);
  void InputUpdate(const InkPenSample *samples, size_t count);

  const Document &document() const { return history_->current(); }
  // The document as the canvas shows it: during an erase gesture, with the
  // erased strokes hidden or cut; otherwise the document.
  const Document &Shown() const { return erase_ ? erase_->shown : document(); }
  const Transform &view() const { return view_; }

  // The stroke being drawn, if any: its page and pen, its outline in page
  // coordinates, and the page-space area its geometry changed in since the
  // last call.
  bool Drawing() const { return live_.has_value(); }
  size_t LivePage() const { return page_; }
  const Pen &LivePen() const { return live_->pen; }
  std::vector<Polyline> LiveOutline() const;
  ink::Envelope TakeUpdatedRegion();

 private:
  struct LiveStroke {
    ink::InProgressStroke stroke;
    InkTool tool;
    double t0 = 0;
    Pen pen;
    Point origin;                    // content position of the page
    std::vector<InkPenSample> real;  // page coordinates in x, y
    bool updated = false;            // ink_input_update changed a real sample
  };
  struct CommittedStroke {
    std::string id;
    size_t page = 0, layer = 0;
    double t0 = 0;
    Pen pen;
    Point origin;
    std::vector<InkPenSample> real;
  };

  // A stroke or shape the free eraser touched: the strokes it is cut from (a
  // shape's strokes along its geometry) and the sections erased from each.
  struct FreeErased {
    std::vector<Stroke> strokes;
    std::vector<ErasedSections> erased;
    std::vector<Stroke> pieces;  // what remains of `strokes`, without ids
    bool stale = false;          // `erased` changed since `pieces`
  };
  // One eraser gesture on one page. Write's split: hit strokes are hidden
  // (tempSelection) and cut strokes replaced by their pieces
  // (freeErasePieces) until the release commits one history step
  // (syncscribble/scribblearea.cpp:1218-1254 freeErase, 1989-2017
  // doReleaseEvent, styluslabs/Write 401b65d).
  struct EraseGesture {
    InkEraser kind = INK_ERASER_STROKE;
    InkTool tool = INK_TOOL_PEN;
    size_t page = 0;
    Point origin;       // content position of the page
    Point last;         // page coordinates
    double radius = 0;  // pt
    std::string time;   // gesture start, for shapes turned into strokes
    std::set<std::string> hit;                    // stroke eraser: ids
    std::map<std::string, FreeErased> free;       // free eraser: by element id
    std::map<std::string, std::vector<ink::Stroke>> meshes;  // hit-test strokes by id
    Document shown;
  };

  bool Erases(const InkPenSample &sample) const;
  void EraseInput(const InkPenSample *samples, size_t count);
  void EraseAlong(Point from, Point to);
  const std::vector<ink::Stroke> &HitStrokes(const std::string &id, const Element &element);
  void UpdateShown();
  void CommitErase();
  // The gesture's page with hit elements removed and cut ones replaced by
  // their pieces; with `ids`, each piece gets a new id.
  Page ErasedPage(IdGenerator *ids) const;

  ink::StrokeInput ToStrokeInput(const InkPenSample &sample, double t0) const;
  ink::StrokeInputBatch Batch(const std::vector<InkPenSample> &samples, double t0) const;
  InkPenSample ToPage(InkPenSample sample, const Point &origin) const;
  void Commit();
  Stroke MakeElement(const std::string &id, const ink::Stroke &ink_stroke, const Pen &pen,
                     double t0, const std::vector<InkPenSample> &real) const;

  DocumentHistory *history_;
  Transform view_;
  Pen pen_;
  double utc_offset_ms_ = 0;
  size_t page_ = 0, layer_ = 0;
  InkEraser eraser_kind_ = INK_ERASER_STROKE;
  bool eraser_active_ = false;
  std::optional<LiveStroke> live_;
  std::optional<EraseGesture> erase_;
  std::vector<CommittedStroke> committed_;
};

}  // namespace ink_engine
