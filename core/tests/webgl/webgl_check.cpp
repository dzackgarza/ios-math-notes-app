// WebGL2 canvas -> GrDirectContext -> SkSurface. Follows Skia
// modules/canvaskit/canvaskit_bindings.cpp:286-340 (MakeGrContext,
// MakeOnScreenGLSurface) and the context attributes of
// modules/canvaskit/webgl.js:14-50.
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include <cstdint>

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLMakeWebGLInterface.h"
#include "include/utils/SkParsePath.h"

namespace {
sk_sp<GrDirectContext> gContext;
sk_sp<SkSurface> gSurface;
}  // namespace

extern "C" {

// Draws a red triangle on white into the canvas `#canvas`. Returns 0 on success.
EMSCRIPTEN_KEEPALIVE int webgl_draw(int width, int height) {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.alpha = 1;
  attrs.depth = 1;
  attrs.stencil = 8;
  attrs.antialias = 0;
  attrs.premultipliedAlpha = 1;
  attrs.preserveDrawingBuffer = 0;
  attrs.enableExtensionsByDefault = 1;
  attrs.majorVersion = 2;
  auto gl = emscripten_webgl_create_context("#canvas", &attrs);
  if (gl <= 0) return 1;
  if (emscripten_webgl_make_context_current(gl) != EMSCRIPTEN_RESULT_SUCCESS) return 2;

  gContext = GrDirectContexts::MakeGL(GrGLInterfaces::MakeWebGL());
  if (!gContext) return 3;

  GLint samples = 0, stencil = 0;
  glGetIntegerv(GL_SAMPLES, &samples);
  glGetIntegerv(GL_STENCIL_BITS, &stencil);
  GrGLFramebufferInfo info;
  info.fFBOID = 0;
  info.fFormat = GL_RGBA8;
  auto target = GrBackendRenderTargets::MakeGL(width, height, samples, stencil, info);
  gSurface = SkSurfaces::WrapBackendRenderTarget(gContext.get(), target,
                                                 kBottomLeft_GrSurfaceOrigin,
                                                 kRGBA_8888_SkColorType,
                                                 SkColorSpace::MakeSRGB(), nullptr);
  if (!gSurface) return 4;

  auto path = SkParsePath::FromSVGString("M10 10 L90 10 L50 90 Z");
  if (!path) return 5;
  SkCanvas *canvas = gSurface->getCanvas();
  canvas->clear(SK_ColorWHITE);
  SkPaint paint;
  paint.setColor(SK_ColorRED);
  canvas->drawPath(*path, paint);
  gContext->flushAndSubmit(gSurface.get());
  return 0;
}

// RGBA of one surface pixel, top-left origin, packed as 0xRRGGBBAA.
EMSCRIPTEN_KEEPALIVE uint32_t webgl_pixel(int x, int y) {
  uint8_t rgba[4] = {};
  SkImageInfo info = SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  if (!gSurface || !gSurface->readPixels(info, rgba, 4, x, y)) return 0;
  return uint32_t(rgba[0]) << 24 | uint32_t(rgba[1]) << 16 | uint32_t(rgba[2]) << 8 | rgba[3];
}

}  // extern "C"

// Mean milliseconds of EnqueueInputs plus UpdateShape for one 16-input frame,
// over a 320-input spiral drawn with google/ink's PressurePen.
#include <chrono>
#include <cmath>

#include "ink/brush/brush.h"
#include "ink/brush/stock_brushes.h"
#include "ink/color/color.h"
#include "ink/strokes/in_progress_stroke.h"
#include "ink/strokes/input/stroke_input_batch.h"
#include "ink/types/duration.h"

extern "C" EMSCRIPTEN_KEEPALIVE double stroke_frame_ms() {
  auto brush = ink::Brush::Create(
      ink::stock_brushes::PressurePen(ink::stock_brushes::PressurePenVersion::kV1),
      ink::Color::Black(), 5, 0.1);
  if (!brush.ok()) return -1;
  ink::InProgressStroke stroke;
  stroke.Start(*brush);
  constexpr int kFrames = 20, kFrameInputs = 16;
  double total_ms = 0;
  for (int f = 0; f < kFrames; ++f) {
    ink::StrokeInputBatch frame;
    for (int j = 0; j < kFrameInputs; ++j) {
      float t = float(f * kFrameInputs + j) / 240.0f;  // 240 Hz pencil
      float r = 20 + 40 * t;
      if (!frame.Append({.tool_type = ink::StrokeInput::ToolType::kStylus,
                         .position = {250 + r * std::cos(8 * t), 250 + r * std::sin(8 * t)},
                         .elapsed_time = ink::Duration32::Seconds(t),
                         .pressure = 0.5f + 0.4f * std::sin(3 * t)})
               .ok()) {
        return -1;
      }
    }
    auto start = std::chrono::steady_clock::now();
    if (!stroke.EnqueueInputs(frame, {}).ok()) return -1;
    if (!stroke.UpdateShape(frame.Get(kFrameInputs - 1).elapsed_time).ok()) return -1;
    total_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  }
  return total_ms / kFrames;
}
