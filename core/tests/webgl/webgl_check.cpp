// Browser checks of the engine on a WebGL2 canvas, driven by Playwright:
// webgl.spec.mjs (a frame through ink_render) and frame.spec.mjs (timings).
#include <emscripten/emscripten.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "editor/canvas.h"
#include "ink.h"
#include "ink/strokes/stroke.h"
#include "strokes/outline.h"

using namespace ink_engine;

namespace {

double Now() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// Waits for the GPU: a one-pixel read blocks until the frame is drawn.
void Finish() {
  uint8_t rgba[4];
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

std::unique_ptr<InkCanvas> gCanvas;
double gWorstMs = 0;

}  // namespace

extern "C" {

// A canvas on `#canvas` (width × height pixels, view = content), with one
// pressure-pen stroke from (40, 50) to (140, 50). Returns ink_render's result
// for the frame after the stroke, or -1 when WebGL2 fails.
EMSCRIPTEN_KEEPALIVE int render_stroke(int width, int height) {
  gCanvas.reset(ink_canvas_create(1));
  if (ink_canvas_attach_webgl(gCanvas.get(), "#canvas") != 0) return -1;
  ink_canvas_set_surface_size(gCanvas.get(), width, height, 1);
  ink_canvas_set_pen(gCanvas.get(), INK_BRUSH_MARKER, 0x1A1A1A, 6);
  ink_render(gCanvas.get());
  std::vector<InkPenSample> samples;
  for (int i = 0; i <= 20; ++i) {
    InkPhase phase = i == 0 ? INK_PHASE_BEGIN : i == 20 ? INK_PHASE_END : INK_PHASE_MOVE;
    samples.push_back({.x = 40.0 + 5 * i, .y = 50, .time = 4.0 * i, .id = uint32_t(i),
                       .tool = INK_TOOL_PEN, .phase = uint8_t(phase)});
  }
  ink_input(gCanvas.get(), samples.data(), samples.size());
  return ink_render(gCanvas.get());
}

// ink_render with nothing changed.
EMSCRIPTEN_KEEPALIVE int render_again() { return ink_render(gCanvas.get()); }

// RGBA of one pixel of the drawing buffer, top-left origin, as 0xRRGGBBAA.
EMSCRIPTEN_KEEPALIVE uint32_t canvas_pixel(int x, int y, int height) {
  uint8_t rgba[4] = {};
  glReadPixels(x, height - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  return uint32_t(rgba[0]) << 24 | uint32_t(rgba[1]) << 16 | uint32_t(rgba[2]) << 8 | rgba[3];
}

// Mean milliseconds of one 16-sample ink_input event, over a 320-sample
// spiral drawn with the pressure pen.
EMSCRIPTEN_KEEPALIVE double stroke_frame_ms() {
  InkCanvas *canvas = ink_canvas_create(1);
  ink_canvas_set_pen(canvas, INK_BRUSH_PRESSURE_PEN, 0x1A1A1A, 5);
  constexpr int kEvents = 20, kSamples = 16;
  double total_ms = 0;
  for (int e = 0; e < kEvents; ++e) {
    std::vector<InkPenSample> event;
    for (int j = 0; j < kSamples; ++j) {
      int n = e * kSamples + j;
      double t = n / 240.0, r = 20 + 40 * t;  // 240 Hz pencil
      event.push_back({.x = 250 + r * std::cos(8 * t), .y = 250 + r * std::sin(8 * t),
                       .time = t * 1000, .pressure = float(0.5 + 0.4 * std::sin(3 * t)),
                       .has = INK_HAS_PRESSURE, .id = uint32_t(n), .tool = INK_TOOL_PEN,
                       .phase = uint8_t(n == 0 ? INK_PHASE_BEGIN : INK_PHASE_MOVE)});
    }
    double start = Now();
    ink_input(canvas, event.data(), event.size());
    total_ms += Now() - start;
  }
  ink_canvas_destroy(canvas);
  return total_ms / kEvents;
}

// Frame times while zooming a 20-page notebook with 400 strokes per page on
// `#canvas` (width × height pixels), about the canvas center over the second
// page: opened at 1×, then 60 frames to 0.5×, to 2×, and back to 1×. Each
// stroke is a handwriting-sized pressure-pen curve of 167 samples. Returns the mean
// frame in ms; zoom_worst_ms gives the worst.
EMSCRIPTEN_KEEPALIVE double zoom_frame_ms(int width, int height) {
  constexpr int kPages = 20, kRows = 25, kColumns = 16, kShapes = 24, kSamples = 167;
  Pen pen{.brush = INK_BRUSH_PRESSURE_PEN, .color = {26, 26, 26}, .size = 1.6f};
  std::vector<Stroke> shapes;
  for (int k = 0; k < kShapes; ++k) {
    ink::StrokeInputBatch batch;
    for (int i = 0; i < kSamples; ++i) {
      double t = i / double(kSamples - 1);  // a looping glyph about 30 × 20 pt
      double x = 30 * t + 4 * std::sin(2 * M_PI * (2 + k % 3) * t);
      double y = 10 - 9 * std::cos(2 * M_PI * (1 + k % 4) * t + k);
      (void)batch.Append({.tool_type = ink::StrokeInput::ToolType::kStylus,
                          .position = {float(x), float(y)},
                          .elapsed_time = ink::Duration32::Millis(float(i * 4)),
                          .pressure = float(0.5 + 0.3 * std::sin(7 * t + k))});
    }
    ink::Stroke stroke(MakeBrush(pen), batch);
    shapes.push_back({.fill = pen.color, .brush = "pressure-pen", .size = pen.size,
                      .outline = StrokeOutline(stroke.GetShape())});
  }

  IdGenerator ids(5);
  Document document = NewNotebook(ids);
  Page blank = *document.pages[0];
  document.pages = {};
  for (int p = 0; p < kPages; ++p) {
    Page page = blank;
    page.id = ids.PageId();
    for (int r = 0; r < kRows; ++r) {
      for (int c = 0; c < kColumns; ++c) {
        Stroke stroke = shapes[(p * 7 + r * 5 + c) % kShapes];
        stroke.id = ids.StrokeId();
        stroke.transform = {1, 0, 0, 1, 40 + c * 33.0, 60 + r * 30.0};
        page.layers[0].elements = page.layers[0].elements.push_back(
            immer::box<Element>(Element{std::move(stroke)}));
      }
    }
    document.pages = document.pages.push_back(immer::box<Page>(std::move(page)));
  }

  std::unique_ptr<InkCanvas> canvas(new InkCanvas{Editor(std::move(document), 9)});
  if (ink_canvas_attach_webgl(canvas.get(), "#canvas") != 0) return -1;
  ink_canvas_set_surface_size(canvas.get(), width, height, 1);
  double center_x = 595.28 / 2, center_y = 841.89 + 9.6 + 841.89 / 2;  // page 2
  // The zoom, as log2 of the scale per frame: 1× -> 0.5× -> 2× -> 1×, 20 frames each.
  constexpr int kLeg = 20;
  constexpr double kKeys[] = {0, -1, 1, 0};
  double total = 0, worst = 0;
  int frames = 0;
  for (int f = 0; f <= 3 * kLeg; ++f) {
    int leg = std::min(f / kLeg, 2);
    double t = (f - leg * kLeg) / double(kLeg);
    double scale = std::exp2(kKeys[leg] + t * (kKeys[leg + 1] - kKeys[leg]));
    ink_canvas_set_view(canvas.get(), scale, 0, 0, scale, width / 2.0 - scale * center_x,
                        height / 2.0 - scale * center_y);
    double start = Now();
    ink_render(canvas.get());
    Finish();
    double ms = Now() - start;
    if (f == 0) continue;  // opening the notebook, before the zoom
    total += ms;
    worst = std::max(worst, ms);
    ++frames;
  }
  gWorstMs = worst;
  return total / frames;
}

EMSCRIPTEN_KEEPALIVE double zoom_worst_ms() { return gWorstMs; }

}  // extern "C"
