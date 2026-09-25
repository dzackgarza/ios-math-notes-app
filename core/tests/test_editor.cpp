// Pen samples through the C ABI to live and committed strokes (issue #19).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

#include "editor/canvas.h"
#include "support/session.h"
#include "ink/strokes/stroke.h"
#include "strokes/outline.h"
#include "support/trace.h"

using namespace ink_engine;

namespace {

constexpr uint32_t kAll = INK_HAS_PRESSURE | INK_HAS_ALTITUDE | INK_HAS_AZIMUTH;


InkPenSample PenSample(double x, double y, double ms, InkPhase phase, uint32_t id = 0) {
  return {.x = x, .y = y, .time = ms, .pressure = 0.5f, .has = INK_HAS_PRESSURE, .id = id,
          .tool = INK_TOOL_PEN, .phase = uint8_t(phase)};
}

// The trace's inputs as InkPenSamples: altitude = π/2 − tilt, azimuth = orientation.
InkPenSample FromInput(const ink::StrokeInput &in, InkPhase phase, bool predicted, uint32_t id) {
  return {.x = in.position.x, .y = in.position.y, .time = in.elapsed_time.ToMillis(),
          .pressure = in.pressure,
          .altitude = std::numbers::pi_v<float> / 2 - in.tilt.ValueInRadians(),
          .azimuth = in.orientation.ValueInRadians(), .has = kAll, .id = id,
          .tool = INK_TOOL_PEN, .phase = uint8_t(phase), .predicted = uint8_t(predicted)};
}

// Sends a trace one frame per event; returns the real samples sent.
std::vector<InkPenSample> Replay(InkCanvas *canvas, const std::vector<ink_test::TraceFrame> &frames) {
  std::vector<InkPenSample> real;
  uint32_t id = 0;
  for (size_t f = 0; f < frames.size(); ++f) {
    std::vector<InkPenSample> event;
    for (size_t i = 0; i < frames[f].real.Size(); ++i) {
      bool last = f + 1 == frames.size() && i + 1 == frames[f].real.Size();
      InkPhase phase = real.empty() ? INK_PHASE_BEGIN : last ? INK_PHASE_END : INK_PHASE_MOVE;
      event.push_back(FromInput(frames[f].real.Get(i), phase, false, id++));
      real.push_back(event.back());
    }
    for (size_t i = 0; i < frames[f].predicted.Size(); ++i) {
      event.push_back(FromInput(frames[f].predicted.Get(i), INK_PHASE_MOVE, true, id++));
    }
    ink_input(canvas, event.data(), event.size());
  }
  return real;
}

// The sample conversion of issue #19, applied directly.
ink::StrokeInputBatch DirectInputs(const std::vector<InkPenSample> &samples) {
  ink::StrokeInputBatch batch;
  for (const InkPenSample &s : samples) {
    REQUIRE(batch
                .Append({.tool_type = ink::StrokeInput::ToolType::kStylus,
                         .position = {float(s.x), float(s.y)},
                         .elapsed_time = ink::Duration32::Millis(float(s.time - samples[0].time)),
                         .pressure = s.pressure,
                         .tilt = ink::Angle::Radians(std::numbers::pi_v<float> / 2 - s.altitude),
                         .orientation = ink::Angle::Radians(s.azimuth)})
                .ok());
  }
  return batch;
}

const Stroke &OnlyStroke(const InkCanvas *canvas) {
  const Elements &elements = canvas->editor.document().pages[0]->layers[0].elements;
  REQUIRE(elements.size() == 1);
  return std::get<Stroke>(elements[0]->value);
}

double MaxX(const std::vector<Polyline> &outline) {
  double x = -1e9;
  for (const auto &line : outline) {
    for (const auto &p : line) x = std::max(x, p.x);
  }
  return x;
}

void RequireClose(const std::vector<Polyline> &a, const std::vector<Polyline> &b, double tol) {
  REQUIRE(a.size() == b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    REQUIRE(a[i].size() == b[i].size());
    for (size_t j = 0; j < a[i].size(); ++j) {
      REQUIRE(std::abs(a[i][j].x - b[i][j].x) <= tol);
      REQUIRE(std::abs(a[i][j].y - b[i][j].y) <= tol);
    }
  }
}

}  // namespace

TEST_CASE("A recorded trace through ink_input commits the stroke built directly") {
  auto frames = ink_test::ReadTrace(INK_FIXTURE_DIR "/ink/spring_shape.trace");
  for (auto [brush, tolerance] : {std::pair{INK_BRUSH_MARKER, 1e-3}, {INK_BRUSH_PRESSURE_PEN, 0.2}}) {
    INFO(BrushName(brush));
    ink_test::Session canvas;
    ink_test::SetTool(canvas.get(), brush, 0x1A1A1A, 5);
    std::vector<InkPenSample> sent = Replay(canvas.get(), frames);
    REQUIRE_FALSE(canvas.canvas->editor.Drawing());
    REQUIRE(canvas.document->history.size() == 2);

    ink::Stroke direct(MakeBrush({.brush = brush, .color = {26, 26, 26}, .size = 5}),
                       DirectInputs(sent));
    const Stroke &committed = OnlyStroke(canvas.get());
    RequireClose(committed.outline, StrokeOutline(direct.GetShape()), tolerance);
    CHECK(committed.samples.size() == direct.GetInputs().Size());
    CHECK(committed.brush == BrushName(brush));
  }
}

TEST_CASE("Predicted samples show in the live stroke and not in the committed one") {
  ink_test::Session canvas;
  ink_test::SetTool(canvas.get(), INK_BRUSH_MARKER, 0x1A1A1A, 5);
  std::vector<InkPenSample> event;
  for (int i = 0; i <= 10; ++i) event.push_back(PenSample(i * 10, 100, i * 8, i ? INK_PHASE_MOVE : INK_PHASE_BEGIN));
  for (int i = 1; i <= 5; ++i) {
    InkPenSample p = PenSample(100 + i * 10, 100, 80 + i * 8, INK_PHASE_MOVE);
    p.predicted = 1;
    event.push_back(p);
  }
  ink_input(canvas.get(), event.data(), event.size());
  REQUIRE(canvas.canvas->editor.Drawing());
  CHECK(MaxX(canvas.canvas->editor.LiveOutline()) >= 145);

  InkPenSample end = PenSample(100, 100, 90, INK_PHASE_END);
  ink_input(canvas.get(), &end, 1);
  const Stroke &committed = OnlyStroke(canvas.get());
  CHECK(MaxX(committed.outline) < 105);
  CHECK(committed.samples.size() == 12);
}

TEST_CASE("ink_input_update after the end phase rebuilds the committed stroke") {
  ink_test::Session canvas;
  ink_test::SetTool(canvas.get(), INK_BRUSH_PRESSURE_PEN, 0x1A1A1A, 5);
  std::vector<InkPenSample> event;
  for (uint32_t i = 0; i <= 20; ++i) {
    InkPhase phase = i == 0 ? INK_PHASE_BEGIN : i == 20 ? INK_PHASE_END : INK_PHASE_MOVE;
    event.push_back(PenSample(i * 10, 100, i * 8, phase, i));
  }
  ink_input(canvas.get(), event.data(), event.size());
  REQUIRE(canvas.document->history.size() == 2);
  std::vector<Polyline> before = OnlyStroke(canvas.get()).outline;

  // UIKit's final force arrives after touchesEnded.
  std::vector<InkPenSample> updates;
  for (uint32_t i = 5; i <= 15; ++i) {
    InkPenSample u = event[i];
    u.pressure = 1.0f;
    updates.push_back(u);
  }
  ink_input_update(canvas.get(), updates.data(), updates.size());
  CHECK(canvas.document->history.size() == 3);
  const Stroke &after = OnlyStroke(canvas.get());
  CHECK(after.outline != before);
  CHECK(after.samples[10].force == 1.0);
}

TEST_CASE("Touch samples during a pen stroke do not reach it") {
  ink_test::Session canvas;
  ink_test::SetTool(canvas.get(), INK_BRUSH_MARKER, 0x1A1A1A, 5);
  InkPenSample begin = PenSample(0, 100, 0, INK_PHASE_BEGIN);
  ink_input(canvas.get(), &begin, 1);
  for (int i = 1; i <= 10; ++i) {
    InkPenSample touch = PenSample(400, 400 + i, i * 8 - 4, i == 1 ? INK_PHASE_BEGIN : INK_PHASE_MOVE);
    touch.tool = INK_TOOL_TOUCH;
    InkPenSample pen = PenSample(i * 10, 100, i * 8, i == 10 ? INK_PHASE_END : INK_PHASE_MOVE);
    InkPenSample event[] = {touch, pen};
    ink_input(canvas.get(), event, 2);
  }
  CHECK(canvas.document->history.size() == 2);
  const Stroke &committed = OnlyStroke(canvas.get());
  CHECK(committed.samples.size() == 11);
  CHECK(MaxX(committed.outline) < 105);
}

TEST_CASE("A highlighter stroke goes under the ink of its layer") {
  ink_test::Session canvas;
  ink_test::SetTool(canvas.get(), INK_BRUSH_MARKER, 0x1A1A1A, 5);
  InkPenSample line[] = {PenSample(0, 100, 0, INK_PHASE_BEGIN), PenSample(50, 100, 10, INK_PHASE_END)};
  ink_input(canvas.get(), line, 2);
  ink_test::SetTool(canvas.get(), INK_BRUSH_HIGHLIGHTER, 0xFFEE00, 10);
  InkPenSample mark[] = {PenSample(0, 200, 100, INK_PHASE_BEGIN), PenSample(50, 200, 110, INK_PHASE_END)};
  ink_input(canvas.get(), mark, 2);
  const Elements &elements = canvas.canvas->editor.document().pages[0]->layers[0].elements;
  REQUIRE(elements.size() == 2);
  CHECK(std::get<Stroke>(elements[0]->value).brush == "highlighter");
  CHECK(std::get<Stroke>(elements[1]->value).brush == "marker");
}

TEST_CASE("One 16-sample event through ink_input") {
  ink_test::Session canvas;
  ink_test::SetTool(canvas.get(), INK_BRUSH_PRESSURE_PEN, 0x1A1A1A, 5);
  double total = 0, worst = 0;
  constexpr int kEvents = 20;
  for (int e = 0; e < kEvents; ++e) {
    std::vector<InkPenSample> event;
    for (int j = 0; j < 16; ++j) {
      int n = e * 16 + j;
      double t = n / 240.0, r = 20 + 40 * t;
      event.push_back(PenSample(250 + r * std::cos(8 * t), 250 + r * std::sin(8 * t), t * 1000,
                          n == 0 ? INK_PHASE_BEGIN : INK_PHASE_MOVE));
    }
    auto start = std::chrono::steady_clock::now();
    ink_input(canvas.get(), event.data(), event.size());
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    total += ms;
    worst = std::max(worst, ms);
  }
  std::printf("16-sample ink_input event: mean %.3f ms, worst %.3f ms\n", total / kEvents, worst);
  CHECK(canvas.canvas->editor.Drawing());
}
