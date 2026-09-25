// The object behind the C ABI's opaque InkCanvas handle.
#pragma once

#include <memory>

#include "editor/editor.h"
#include "render/host_surface.h"
#include "render/renderer.h"

namespace ink_engine {

// A new notebook: one layer and one blank A4 page (FORMAT.md).
Document NewNotebook(IdGenerator &ids);

}  // namespace ink_engine

struct InkCanvas {
  ink_engine::Editor editor;
  std::unique_ptr<ink_engine::HostSurface> surface;
  std::unique_ptr<ink_engine::Renderer> renderer;
  int width = 0, height = 0;  // device pixels
  float pixel_ratio = 1;
  bool was_drawing = false;
};
