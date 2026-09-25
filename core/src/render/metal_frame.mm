// CAMetalLayer -> SkSurface. Follows Skia tools/window/MetalWindowContext.mm:35-135
// (context setup, WrapCAMetalLayer, present) and
// experimental/minimal_ios_mtl_skia_app/main.mm:55-122.
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "ink.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/SkSurfaceMetal.h"
#include "include/utils/SkParsePath.h"

extern "C" int ink_metal_draw_test_frame(void *ca_metal_layer) {
  CAMetalLayer *layer = (__bridge CAMetalLayer *)ca_metal_layer;
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) return 1;
  id<MTLCommandQueue> queue = [device newCommandQueue];
  layer.device = device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

  // sk_cfp takes ownership without retaining: retain explicitly under ARC.
  GrMtlBackendContext backend = {};
  backend.fDevice.retain((__bridge GrMTLHandle)device);
  backend.fQueue.retain((__bridge GrMTLHandle)queue);
  sk_sp<GrDirectContext> context = GrDirectContexts::MakeMetal(backend);
  if (!context) return 2;

  GrMTLHandle drawableHandle = nullptr;
  sk_sp<SkSurface> surface = SkSurfaces::WrapCAMetalLayer(
      context.get(), (__bridge GrMTLHandle)layer, kTopLeft_GrSurfaceOrigin, 1,
      kBGRA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr, &drawableHandle);
  if (!surface) return 3;

  auto path = SkParsePath::FromSVGString("M10 10 L90 10 L50 90 Z");
  if (!path) return 4;
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(SK_ColorWHITE);
  SkPaint paint;
  paint.setColor(SK_ColorRED);
  canvas->drawPath(*path, paint);
  context->flushAndSubmit(surface.get());

  // Skia returns the drawable handle +1 (CFRetain); ARC takes it over.
  id<CAMetalDrawable> drawable = (__bridge_transfer id<CAMetalDrawable>)drawableHandle;
  if (!drawable) return 5;
  id<MTLCommandBuffer> commands = [queue commandBuffer];
  [commands presentDrawable:drawable];
  [commands commit];
  [commands waitUntilCompleted];
  return commands.status == MTLCommandBufferStatusCompleted ? 0 : 6;
}
