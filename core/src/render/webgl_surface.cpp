// WebGL2 canvas -> GrDirectContext -> SkSurface. Follows Skia
// modules/canvaskit/canvaskit_bindings.cpp:286-340 (MakeGrContext,
// MakeOnScreenGLSurface) and the context attributes of
// modules/canvaskit/webgl.js:14-50.
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLMakeWebGLInterface.h"
#include "render/host_surface.h"

namespace ink_engine {
namespace {

class WebGLSurface : public HostSurface {
 public:
  WebGLSurface(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl, sk_sp<GrDirectContext> context)
      : gl_(gl), context_(std::move(context)) {}
  ~WebGLSurface() override {
    surface_.reset();
    context_.reset();
    emscripten_webgl_destroy_context(gl_);
  }

  GrDirectContext *context() override { return context_.get(); }

  SkSurface *BeginFrame(int width, int height) override {
    emscripten_webgl_make_context_current(gl_);
    if (!surface_ || surface_->width() != width || surface_->height() != height) {
      GLint samples = 0, stencil = 0;
      glGetIntegerv(GL_SAMPLES, &samples);
      glGetIntegerv(GL_STENCIL_BITS, &stencil);
      GrGLFramebufferInfo info;
      info.fFBOID = 0;
      info.fFormat = GL_RGBA8;
      auto target = GrBackendRenderTargets::MakeGL(width, height, samples, stencil, info);
      surface_ = SkSurfaces::WrapBackendRenderTarget(context_.get(), target,
                                                     kBottomLeft_GrSurfaceOrigin,
                                                     kRGBA_8888_SkColorType,
                                                     SkColorSpace::MakeSRGB(), nullptr);
    }
    return surface_.get();
  }

  // The browser presents the drawing buffer when control returns to it.
  void EndFrame() override { context_->flushAndSubmit(surface_.get()); }

 private:
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl_;
  sk_sp<GrDirectContext> context_;
  sk_sp<SkSurface> surface_;
};

}  // namespace

std::unique_ptr<HostSurface> MakeWebGLSurface(const char *selector) {
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
  auto gl = emscripten_webgl_create_context(selector, &attrs);
  if (gl <= 0) return nullptr;
  if (emscripten_webgl_make_context_current(gl) != EMSCRIPTEN_RESULT_SUCCESS) return nullptr;
  sk_sp<GrDirectContext> context = GrDirectContexts::MakeGL(GrGLInterfaces::MakeWebGL());
  if (!context) {
    emscripten_webgl_destroy_context(gl);
    return nullptr;
  }
  return std::make_unique<WebGLSurface>(gl, std::move(context));
}

}  // namespace ink_engine
