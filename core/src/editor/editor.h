// An editing session on one document: pen input to live and committed
// strokes, and the history of document values (docs/ARCHITECTURE.md: undo
// and redo move an index in a list of document values).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "document/document.h"
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

  void Input(const InkPenSample *samples, size_t count);
  void InputUpdate(const InkPenSample *samples, size_t count);

  const Document &document() const { return history_->current(); }
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
  std::optional<LiveStroke> live_;
  std::vector<CommittedStroke> committed_;
};

}  // namespace ink_engine
