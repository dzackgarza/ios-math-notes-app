// Live strokes follow google/ink in_progress_stroke.h:106-261 (Start,
// EnqueueInputs(real, predicted), UpdateShape, FinishInputs, CopyToStroke).
#include "editor/editor.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>

#include "ink/brush/stock_brushes.h"
#include "ink/color/color.h"
#include "ink/strokes/stroke.h"
#include "strokes/outline.h"

namespace ink_engine {
namespace {

constexpr float kBrushEpsilon = 0.01f;  // pt

ink::StrokeInput::ToolType ToolType(uint8_t tool) {
  switch (tool) {
    case INK_TOOL_MOUSE: return ink::StrokeInput::ToolType::kMouse;
    case INK_TOOL_TOUCH: return ink::StrokeInput::ToolType::kTouch;
    default: return ink::StrokeInput::ToolType::kStylus;
  }
}

// UTC "YYYY-MM-DDTHH:MM:SS.mmmZ".
std::string IsoTime(double utc_ms) {
  using namespace std::chrono;
  auto ms = milliseconds(int64_t(std::llround(utc_ms)));
  sys_days day = floor<days>(sys_time<milliseconds>(ms));
  year_month_day ymd(day);
  hh_mm_ss<milliseconds> time(ms - day.time_since_epoch());
  char text[32];
  std::snprintf(text, sizeof text, "%04d-%02u-%02uT%02lld:%02lld:%02lld.%03lldZ", int(ymd.year()),
                unsigned(ymd.month()), unsigned(ymd.day()), (long long)time.hours().count(),
                (long long)time.minutes().count(), (long long)time.seconds().count(),
                (long long)time.subseconds().count());
  return text;
}

uint32_t Channels(uint32_t has) {
  uint32_t channels = kChannelX | kChannelY | kChannelT;
  if (has & INK_HAS_PRESSURE) channels |= kChannelF;
  if (has & INK_HAS_ALTITUDE) channels |= kChannelOE;
  if (has & INK_HAS_AZIMUTH) channels |= kChannelOA;
  if (has & INK_HAS_ROLL) channels |= kChannelOR;
  return channels;
}

Transform Inverse(const Transform &m) {
  double det = m.a * m.d - m.b * m.c;
  return {m.d / det,
          -m.b / det,
          -m.c / det,
          m.a / det,
          (m.c * m.f - m.d * m.e) / det,
          (m.b * m.e - m.a * m.f) / det};
}

// Visits every element box of a layer, with a way to replace it.
bool ReplaceStroke(Elements &elements, const std::string &id, const Stroke &replacement) {
  for (size_t i = 0; i < elements.size(); ++i) {
    const Element &element = *elements[i];
    if (auto *s = std::get_if<Stroke>(&element.value); s && s->id == id) {
      elements = elements.set(i, immer::box<Element>(Element{replacement}));
      return true;
    }
  }
  return false;
}

}  // namespace

const char *BrushName(InkBrush brush) {
  switch (brush) {
    case INK_BRUSH_MARKER: return "marker";
    case INK_BRUSH_HIGHLIGHTER: return "highlighter";
    default: return "pressure-pen";
  }
}

ink::Brush MakeBrush(const Pen &pen) {
  using namespace ink::stock_brushes;
  ink::BrushFamily family =
      pen.brush == INK_BRUSH_MARKER ? Marker(MarkerVersion::kV1)
      : pen.brush == INK_BRUSH_HIGHLIGHTER
          ? Highlighter(ink::BrushPaint::SelfOverlap::kDiscard, HighlighterVersion::kV1)
          : PressurePen(PressurePenVersion::kV1);
  ink::Color color = ink::Color::FromUint8(pen.color.r, pen.color.g, pen.color.b, 255);
  return *ink::Brush::Create(family, color, pen.size, kBrushEpsilon);
}

Editor::Editor(Document document, uint64_t id_seed) : ids_(id_seed) {
  history_.push_back(std::move(document));
}

InkPenSample Editor::ToPage(InkPenSample sample) const {
  Transform inverse = Inverse(view_);
  double x = sample.x, y = sample.y;
  sample.x = inverse.a * x + inverse.c * y + inverse.e;
  sample.y = inverse.b * x + inverse.d * y + inverse.f;
  return sample;
}

// InkPenSample -> ink::StrokeInput. A value without its capability bit keeps
// google/ink's kNo* sentinel (ink/strokes/input/stroke_input.h:41-100).
ink::StrokeInput Editor::ToStrokeInput(const InkPenSample &s, double t0) const {
  ink::StrokeInput input{
      .tool_type = ToolType(s.tool),
      .position = {float(s.x), float(s.y)},
      .elapsed_time = ink::Duration32::Millis(float(s.time - t0)),
  };
  if (s.has & INK_HAS_PRESSURE) input.pressure = s.pressure;
  if (s.has & INK_HAS_ALTITUDE) {
    input.tilt = ink::Angle::Radians(std::numbers::pi_v<float> / 2 - s.altitude);
  }
  if (s.has & INK_HAS_AZIMUTH) input.orientation = ink::Angle::Radians(s.azimuth);
  if (s.has & INK_HAS_ROLL) input.barrel_twist = ink::Angle::Radians(s.roll);
  return input;
}

ink::StrokeInputBatch Editor::Batch(const std::vector<InkPenSample> &samples, double t0) const {
  ink::StrokeInputBatch batch;
  for (const InkPenSample &s : samples) {
    // Append rejects inputs out of order in time or position; they are dropped,
    // as InProgressStroke::EnqueueInputs does.
    (void)batch.Append(ToStrokeInput(s, t0));
  }
  return batch;
}

void Editor::Input(const InkPenSample *samples, size_t count) {
  std::vector<InkPenSample> real, predicted;
  bool ended = false, cancelled = false;
  for (size_t i = 0; i < count; ++i) {
    const InkPenSample &s = samples[i];
    // Fingers pan and zoom (the host's job); while a pen is down they never draw.
    if (s.tool == INK_TOOL_TOUCH || s.phase == INK_PHASE_HOVER) continue;
    if (s.phase == INK_PHASE_BEGIN) {
      live_.emplace(LiveStroke{.tool = InkTool(s.tool), .t0 = s.time, .pen = pen_});
      live_->stroke.Start(MakeBrush(pen_));
    }
    if (!live_ || s.tool != live_->tool) continue;
    if (s.phase == INK_PHASE_CANCEL) {
      cancelled = true;
      break;
    }
    InkPenSample page = ToPage(s);
    if (s.predicted) {
      predicted.push_back(page);
    } else {
      real.push_back(page);
    }
    if (s.phase == INK_PHASE_END) {
      ended = true;
      break;
    }
  }
  if (cancelled) {
    live_.reset();
    return;
  }
  if (!live_) return;

  live_->real.insert(live_->real.end(), real.begin(), real.end());
  // The pen-up batch carries no prediction: google/ink keeps the smoothing of
  // the last real inputs over predicted ones (TRAPS.md).
  if (ended) predicted.clear();
  (void)live_->stroke.EnqueueInputs(Batch(real, live_->t0), Batch(predicted, live_->t0));
  if (!live_->real.empty()) {
    (void)live_->stroke.UpdateShape(
        ink::Duration32::Millis(float(live_->real.back().time - live_->t0)));
  }
  if (ended) Commit();
}

void Editor::Commit() {
  LiveStroke live = std::move(*live_);
  live_.reset();
  live.stroke.FinishInputs();
  (void)live.stroke.UpdateShape(ink::Duration32::Infinite());
  // Samples replaced by ink_input_update after they were enqueued: build the
  // stroke from the final samples instead.
  ink::Stroke ink_stroke = live.updated
                               ? ink::Stroke(MakeBrush(live.pen), Batch(live.real, live.t0))
                               : live.stroke.CopyToStroke();
  if (ink_stroke.GetInputs().IsEmpty()) return;

  std::string id = ids_.StrokeId();
  Stroke element = MakeElement(id, ink_stroke, live.pen, live.t0, live.real);
  Document next = document();
  Page page = *next.pages[page_];
  Elements &elements = page.layers[layer_].elements;
  auto box = immer::box<Element>(Element{std::move(element)});
  // A highlighter goes under the ink of its layer, as Write's DRAW_UNDER does
  // (syncscribble/scribblearea.cpp:1975-1976, styluslabs/Write 401b65d).
  elements = live.pen.brush == INK_BRUSH_HIGHLIGHTER ? std::move(elements).push_front(box)
                                                     : std::move(elements).push_back(box);
  next.pages = next.pages.set(page_, immer::box<Page>(std::move(page)));
  Push(std::move(next));
  committed_.push_back({id, page_, layer_, live.t0, live.pen, std::move(live.real)});
}

Stroke Editor::MakeElement(const std::string &id, const ink::Stroke &ink_stroke, const Pen &pen,
                           double t0, const std::vector<InkPenSample> &real) const {
  Stroke element{
      .id = id,
      .fill = pen.color,
      .brush = BrushName(pen.brush),
      .brush_version = 1,
      .size = pen.size,
      .time = IsoTime(t0 + utc_offset_ms_),
      .outline = StrokeOutline(ink_stroke.GetShape()),
      .channels = Channels(real.empty() ? 0 : real.front().has),
  };
  for (const InkPenSample &s : real) {
    element.samples.push_back({.x = s.x, .y = s.y, .t = s.time - t0, .force = s.pressure,
                               .altitude = s.altitude, .azimuth = s.azimuth, .roll = s.roll});
  }
  return element;
}

void Editor::InputUpdate(const InkPenSample *samples, size_t count) {
  std::map<size_t, bool> changed;  // committed stroke index
  for (size_t i = 0; i < count; ++i) {
    InkPenSample update = ToPage(samples[i]);
    auto replace = [&](std::vector<InkPenSample> &list) {
      for (InkPenSample &s : list) {
        if (s.id != update.id) continue;
        update.time = s.time;  // the sample keeps its place in time
        update.phase = s.phase;
        s = update;
        return true;
      }
      return false;
    };
    if (live_ && replace(live_->real)) {
      live_->updated = true;
      continue;
    }
    for (size_t k = committed_.size(); k-- > 0;) {
      if (replace(committed_[k].real)) {
        changed[k] = true;
        break;
      }
    }
  }
  if (changed.empty()) return;

  // One history step for every stroke this batch changed.
  Document next = document();
  for (const auto &[k, _] : changed) {
    const CommittedStroke &c = committed_[k];
    ink::Stroke rebuilt(MakeBrush(c.pen), Batch(c.real, c.t0));
    Page page = *next.pages[c.page];
    Stroke element = MakeElement(c.id, rebuilt, c.pen, c.t0, c.real);
    if (!ReplaceStroke(page.layers[c.layer].elements, c.id, element)) continue;
    next.pages = next.pages.set(c.page, immer::box<Page>(std::move(page)));
  }
  Push(std::move(next));
}

std::vector<Polyline> Editor::LiveOutline() const {
  return live_ ? ink_engine::LiveOutline(live_->stroke) : std::vector<Polyline>();
}

ink::Envelope Editor::TakeUpdatedRegion() {
  if (!live_) return {};
  ink::Envelope region = live_->stroke.GetUpdatedRegion();
  live_->stroke.ResetUpdatedRegion();
  return region;
}

void Editor::Push(Document next) {
  history_.resize(index_ + 1);  // a new step drops the redo branch
  history_.push_back(std::move(next));
  index_ = history_.size() - 1;
}

}  // namespace ink_engine
