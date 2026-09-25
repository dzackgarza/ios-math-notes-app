// The objects behind the C ABI's opaque handles.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "document/pages.h"
#include "editor/editor.h"
#include "editor/history.h"
#include "render/host_surface.h"
#include "render/renderer.h"

struct InkDocument {
  ink_engine::DocumentHistory history;
  ink_engine::Assets assets;
  uint64_t assets_version = 0;  // counts ink_document_load_asset calls
  // Page 1 of the notebook's template notebook (ink_document_set_template).
  std::optional<ink_engine::Page> template_page;
  // The last ink_document_dirty_files result, which the host reads in place.
  std::vector<std::pair<std::string, std::string>> dirty;
  std::vector<std::string> dirty_removed;
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
