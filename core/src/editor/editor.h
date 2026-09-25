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
#include "document/ids.h"
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
  Editor(Document document, uint64_t id_seed);

  // page -> view affine transform, SVG matrix order.
  void SetView(const Transform &page_to_view) { view_ = page_to_view; }
  void SetPen(const Pen &pen) { pen_ = pen; }
  // Added to host sample times (ms) to get UTC ms since the Unix epoch.
  void SetUtcOffset(double utc_minus_host_ms) { utc_offset_ms_ = utc_minus_host_ms; }

  void Input(const InkPenSample *samples, size_t count);
  void InputUpdate(const InkPenSample *samples, size_t count);

  const Document &document() const { return history_[index_]; }
  size_t HistorySize() const { return history_.size(); }

  // The stroke being drawn, if any: its outline in page coordinates and the
  // page-space area its geometry changed in since the last call.
  bool Drawing() const { return live_.has_value(); }
  std::vector<Polyline> LiveOutline() const;
  ink::Envelope TakeUpdatedRegion();

 private:
  struct LiveStroke {
    ink::InProgressStroke stroke;
    InkTool tool;
    double t0 = 0;
    Pen pen;
    std::vector<InkPenSample> real;  // page coordinates in x, y
    bool updated = false;            // ink_input_update changed a real sample
  };
  struct CommittedStroke {
    std::string id;
    size_t page = 0, layer = 0;
    double t0 = 0;
    Pen pen;
    std::vector<InkPenSample> real;
  };

  ink::StrokeInput ToStrokeInput(const InkPenSample &sample, double t0) const;
  ink::StrokeInputBatch Batch(const std::vector<InkPenSample> &samples, double t0) const;
  InkPenSample ToPage(InkPenSample sample) const;
  void Commit();
  Stroke MakeElement(const std::string &id, const ink::Stroke &ink_stroke, const Pen &pen,
                     double t0, const std::vector<InkPenSample> &real) const;
  void Push(Document next);

  std::vector<Document> history_;
  size_t index_ = 0;
  IdGenerator ids_;
  Transform view_;
  Pen pen_;
  double utc_offset_ms_ = 0;
  size_t page_ = 0, layer_ = 0;
  std::optional<LiveStroke> live_;
  std::vector<CommittedStroke> committed_;
};

}  // namespace ink_engine
