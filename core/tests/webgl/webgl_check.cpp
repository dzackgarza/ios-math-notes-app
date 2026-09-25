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

// Mean milliseconds of one 16-sample ink_input event, over a 320-sample
// spiral drawn with the pressure pen.
#include <chrono>
#include <cmath>
#include <vector>

#include "ink.h"

extern "C" EMSCRIPTEN_KEEPALIVE double stroke_frame_ms() {
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
    auto start = std::chrono::steady_clock::now();
    ink_input(canvas, event.data(), event.size());
    total_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  }
  ink_canvas_destroy(canvas);
  return total_ms / kEvents;
}
