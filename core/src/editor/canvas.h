// The objects behind the C ABI's opaque handles.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "editor/editor.h"
#include "editor/history.h"
#include "render/host_surface.h"
#include "render/renderer.h"

namespace ink_engine {

// A new notebook: one layer and one blank A4 page (FORMAT.md).
Document NewNotebook(IdGenerator &ids);

}  // namespace ink_engine

struct InkDocument {
  ink_engine::DocumentHistory history;
  ink_engine::Assets assets;
  uint64_t assets_version = 0;  // counts ink_document_load_asset calls
  // The last ink_document_dirty_files result, which the host reads in place.
  std::vector<std::pair<std::string, std::string>> dirty;
  std::vector<InkFile> dirty_view;
};

struct InkCanvas {
  explicit InkCanvas(InkDocument &doc) : document(&doc), editor(doc.history) {}

  InkDocument *document;
  ink_engine::Editor editor;
  std::unique_ptr<ink_engine::HostSurface> surface;
  std::unique_ptr<ink_engine::Renderer> renderer;
  int width = 0, height = 0;  // device pixels
  float pixel_ratio = 1;
  bool was_drawing = false;
  uint64_t assets_seen = 0;
};
