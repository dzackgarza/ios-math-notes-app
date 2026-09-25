// CAMetalLayer -> SkSurface. Follows Skia tools/window/MetalWindowContext.mm:35-135
// (context setup, WrapCAMetalLayer per frame, present) and
// experimental/minimal_ios_mtl_skia_app/main.mm:55-122.
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/SkSurfaceMetal.h"
#include "render/host_surface.h"

namespace ink_engine {
namespace {

class MetalSurface : public HostSurface {
 public:
  MetalSurface(CAMetalLayer *layer, id<MTLCommandQueue> queue, sk_sp<GrDirectContext> context)
      : layer_(layer), queue_(queue), context_(std::move(context)) {}

  GrDirectContext *context() override { return context_.get(); }

  SkSurface *BeginFrame(int width, int height) override {
    // drawableSize is in pixels: the view's bounds × contentScaleFactor.
    layer_.drawableSize = CGSizeMake(width, height);
    drawable_ = nullptr;
    surface_ = SkSurfaces::WrapCAMetalLayer(context_.get(), (__bridge GrMTLHandle)layer_,
                                            kTopLeft_GrSurfaceOrigin, 1, kBGRA_8888_SkColorType,
                                            SkColorSpace::MakeSRGB(), nullptr, &drawable_);
    return surface_.get();
  }

  void EndFrame() override {
    context_->flushAndSubmit(surface_.get());
    surface_.reset();
    // Skia returns the drawable handle +1 (CFRetain); ARC takes it over.
    id<CAMetalDrawable> drawable = (__bridge_transfer id<CAMetalDrawable>)drawable_;
    drawable_ = nullptr;
    if (!drawable) return;
    id<MTLCommandBuffer> commands = [queue_ commandBuffer];
    [commands presentDrawable:drawable];
    [commands commit];
  }

 private:
  CAMetalLayer *layer_;
  id<MTLCommandQueue> queue_;
  sk_sp<GrDirectContext> context_;
  sk_sp<SkSurface> surface_;
  GrMTLHandle drawable_ = nullptr;
};

}  // namespace

std::unique_ptr<HostSurface> MakeMetalSurface(void *mtl_device, void *mtl_queue,
                                              void *ca_metal_layer) {
  id<MTLDevice> device = (__bridge id<MTLDevice>)mtl_device;
  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)mtl_queue;
  CAMetalLayer *layer = (__bridge CAMetalLayer *)ca_metal_layer;
  layer.device = device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

  // sk_cfp takes ownership without retaining: retain explicitly under ARC.
  GrMtlBackendContext backend = {};
  backend.fDevice.retain((__bridge GrMTLHandle)device);
  backend.fQueue.retain((__bridge GrMTLHandle)queue);
  sk_sp<GrDirectContext> context = GrDirectContexts::MakeMetal(backend);
  if (!context) return nullptr;
  return std::make_unique<MetalSurface>(layer, queue, std::move(context));
}

}  // namespace ink_engine
