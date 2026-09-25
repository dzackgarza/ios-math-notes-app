// The screen a canvas draws on: a WebGL2 canvas element or a CAMetalLayer.
#pragma once

#include <memory>

class GrDirectContext;
class SkSurface;

namespace ink_engine {

class HostSurface {
 public:
  virtual ~HostSurface() = default;
  virtual GrDirectContext *context() = 0;
  // The surface for the next frame, `width` × `height` device pixels; null
  // when the platform has none to give.
  virtual SkSurface *BeginFrame(int width, int height) = 0;
  // Submits the frame's GPU work and presents it.
  virtual void EndFrame() = 0;
};

#ifdef __EMSCRIPTEN__
// The WebGL2 canvas matched by the CSS `selector`; null when WebGL2 fails.
std::unique_ptr<HostSurface> MakeWebGLSurface(const char *selector);
#endif
#ifdef __APPLE__
// An MTLDevice, its MTLCommandQueue, and a CAMetalLayer; null when Skia
// cannot make a Metal context.
std::unique_ptr<HostSurface> MakeMetalSurface(void *mtl_device, void *mtl_queue,
                                              void *ca_metal_layer);
#endif

}  // namespace ink_engine
