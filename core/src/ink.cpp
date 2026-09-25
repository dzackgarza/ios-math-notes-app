#include "ink.h"

#include <algorithm>
#include <exception>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "editor/canvas.h"
#include "format/notebook.h"
#include "layout/layout.h"
#include "include/core/SkData.h"
#include "include/core/SkSurface.h"

namespace ink_engine {

Document NewNotebook(IdGenerator &ids) {
  Document document;
  document.notebook.layers.push_back({.id = ids.LayerId(), .name = "Ink"});
  Page page{.id = ids.PageId(), .file = "pages/0001.svg", .width = 595.28, .height = 841.89};
  page.background.y_ruling = 28.8;  // Write's blankYRuling
  page.layers.push_back({.layer_id = document.notebook.layers[0].id});
  document.pages = document.pages.push_back(immer::box<Page>(std::move(page)));
  return document;
}

}  // namespace ink_engine

namespace {

std::string gLastError;

InkStatus Fail(InkStatus status, std::string message) {
  gLastError = std::move(message);
  return status;
}

// Runs one ABI call: a C++ exception becomes a status and a message.
template <class Body>
InkStatus Call(Body &&body) {
  try {
    return body();
  } catch (const nlohmann::json::exception &e) {
    return Fail(INK_ERROR_PARSE, e.what());
  } catch (const std::exception &e) {
    return Fail(INK_ERROR_INTERNAL, e.what());
  } catch (...) {
    return Fail(INK_ERROR_INTERNAL, "unknown exception");
  }
}

InkStatus NullArgument(const char *name) {
  return Fail(INK_ERROR_ARGUMENT, std::string(name) + " is null");
}

std::string_view Bytes(const uint8_t *bytes, size_t size) {
  return {reinterpret_cast<const char *>(bytes), size};
}

InkStatus AttachSurface(InkDocument *document, std::unique_ptr<ink_engine::HostSurface> surface,
                        InkCanvas **out) {
  if (!surface) return Fail(INK_ERROR_GPU, "no GPU context for the surface");
  auto canvas = std::make_unique<InkCanvas>(*document);
  canvas->surface = std::move(surface);
  canvas->renderer =
      std::make_unique<ink_engine::Renderer>(canvas->surface->context(), document->assets);
  canvas->assets_seen = document->assets_version;
  *out = canvas.release();
  return INK_OK;
}

}  // namespace

extern "C" {

const char *ink_version(void) { return INK_VERSION; }

const char *ink_last_error(void) { return gLastError.c_str(); }

// ---- Documents -----------------------------------------------------------

InkStatus ink_document_create(uint64_t seed, InkDocument **out) {
  return Call([&] {
    if (!out) return NullArgument("out");
    ink_engine::IdGenerator ids(seed);
    ink_engine::Document document = ink_engine::NewNotebook(ids);
    *out = new InkDocument{ink_engine::DocumentHistory(std::move(document), seed + 1)};
    return INK_OK;
  });
}

InkStatus ink_document_load_notebook(InkDocument *document, const uint8_t *json, size_t size) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!json && size) return NullArgument("json");
    document->history.Reset(ink_engine::ReadNotebookJson(Bytes(json, size)));
    return INK_OK;
  });
}

InkStatus ink_document_load_page(InkDocument *document, const char *file, const uint8_t *svg,
                                 size_t size) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!file) return NullArgument("file");
    if (!svg && size) return NullArgument("svg");
    ink_engine::Document next = document->history.current();
    const ink_engine::Page &page = ink_engine::AddPage(next, file, Bytes(svg, size));
    std::optional<std::string> error = page.error;
    document->history.Reset(std::move(next));
    if (error) return Fail(INK_ERROR_PARSE, std::string(file) + ": " + *error);
    return INK_OK;
  });
}

InkStatus ink_document_load_asset(InkDocument *document, const char *path, const uint8_t *bytes,
                                  size_t size) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!path) return NullArgument("path");
    if (!bytes && size) return NullArgument("bytes");
    document->assets[path] = SkData::MakeWithCopy(bytes, size);
    ++document->assets_version;
    return INK_OK;
  });
}

InkStatus ink_document_dirty_files(InkDocument *document, const InkFile **files, size_t *count) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!files || !count) return NullArgument("files");
    const ink_engine::DocumentHistory &history = document->history;
    ink_engine::NotebookFiles changed = ink_engine::ChangedFiles(history.current(), history.saved());
    document->dirty.assign(changed.begin(), changed.end());
    document->dirty_view.clear();
    for (const auto &[path, bytes] : document->dirty) {
      document->dirty_view.push_back({path.c_str(), reinterpret_cast<const uint8_t *>(bytes.data()),
                                      bytes.size()});
    }
    *files = document->dirty_view.data();
    *count = document->dirty_view.size();
    return INK_OK;
  });
}

InkStatus ink_document_mark_saved(InkDocument *document) {
  return Call([&] {
    if (!document) return NullArgument("document");
    document->history.MarkSaved();
    return INK_OK;
  });
}

InkStatus ink_document_content_size(InkDocument *document, double *width, double *height) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!width || !height) return NullArgument("width or height");
    std::vector<ink_engine::PagePlacement> layout =
        ink_engine::LayoutPages(document->history.current());
    *width = 0;
    *height = layout.empty() ? 0 : layout.back().y + layout.back().height;
    for (const auto &page : layout) *width = std::max(*width, page.width);
    return INK_OK;
  });
}

InkStatus ink_document_free(InkDocument *document) {
  return Call([&] {
    delete document;
    return INK_OK;
  });
}

// ---- Canvases ------------------------------------------------------------

#ifdef __EMSCRIPTEN__
InkStatus ink_canvas_create_webgl(InkDocument *document, const char *selector, InkCanvas **out) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!selector) return NullArgument("selector");
    if (!out) return NullArgument("out");
    return AttachSurface(document, ink_engine::MakeWebGLSurface(selector), out);
  });
}
#endif

#ifdef __APPLE__
InkStatus ink_canvas_create_metal(InkDocument *document, void *device, void *queue, void *layer,
                                  InkCanvas **out) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!device || !queue || !layer) return NullArgument("device, queue or layer");
    if (!out) return NullArgument("out");
    return AttachSurface(document, ink_engine::MakeMetalSurface(device, queue, layer), out);
  });
}
#endif

InkStatus ink_canvas_set_view(InkCanvas *canvas, double a, double b, double c, double d, double e,
                              double f) {
  return Call([&] {
    if (!canvas) return NullArgument("canvas");
    if (a * d - b * c == 0) return Fail(INK_ERROR_ARGUMENT, "the view transform is singular");
    canvas->editor.SetView({a, b, c, d, e, f});
    return INK_OK;
  });
}

InkStatus ink_canvas_set_surface_size(InkCanvas *canvas, int32_t width, int32_t height,
                                      float pixel_ratio) {
  return Call([&] {
    if (!canvas) return NullArgument("canvas");
    if (width < 0 || height < 0 || !(pixel_ratio > 0)) {
      return Fail(INK_ERROR_ARGUMENT, "negative surface size or non-positive pixel ratio");
    }
    canvas->width = width;
    canvas->height = height;
    canvas->pixel_ratio = pixel_ratio;
    return INK_OK;
  });
}

InkStatus ink_canvas_set_tool(InkCanvas *canvas, const InkToolSettings *tool) {
  return Call([&] {
    if (!canvas) return NullArgument("canvas");
    if (!tool) return NullArgument("tool");
    if (tool->brush > INK_BRUSH_HIGHLIGHTER) return Fail(INK_ERROR_ARGUMENT, "unknown brush");
    if (!(tool->size > 0)) return Fail(INK_ERROR_ARGUMENT, "non-positive size");
    uint32_t rgb = tool->rgb;
    canvas->editor.SetPen({.brush = InkBrush(tool->brush),
                           .color = {uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb)},
                           .size = tool->size});
    return INK_OK;
  });
}

InkStatus ink_canvas_set_utc_offset(InkCanvas *canvas, double utc_minus_host_ms) {
  return Call([&] {
    if (!canvas) return NullArgument("canvas");
    canvas->editor.SetUtcOffset(utc_minus_host_ms);
    return INK_OK;
  });
}

InkStatus ink_canvas_free(InkCanvas *canvas) {
  return Call([&] {
    delete canvas;
    return INK_OK;
  });
}

InkStatus ink_input(InkCanvas *canvas, const InkPenSample *samples, size_t count) {
  return Call([&] {
    if (!canvas) return NullArgument("canvas");
    if (!samples && count) return NullArgument("samples");
    canvas->editor.Input(samples, count);
    return INK_OK;
  });
}

InkStatus ink_input_update(InkCanvas *canvas, const InkPenSample *samples, size_t count) {
  return Call([&] {
    if (!canvas) return NullArgument("canvas");
    if (!samples && count) return NullArgument("samples");
    canvas->editor.InputUpdate(samples, count);
    return INK_OK;
  });
}

// ---- Frame and history ---------------------------------------------------

InkStatus ink_render(InkCanvas *canvas, int32_t *drew) {
  return Call([&] {
    if (!canvas) return NullArgument("canvas");
    if (!drew) return NullArgument("drew");
    *drew = 0;
    if (canvas->width <= 0 || canvas->height <= 0) return INK_OK;
    ink_engine::Editor &editor = canvas->editor;
    if (canvas->assets_seen != canvas->document->assets_version) {
      canvas->renderer->Invalidate();
      canvas->assets_seen = canvas->document->assets_version;
    }
    bool drawing = editor.Drawing();
    bool live_changed = !editor.TakeUpdatedRegion().IsEmpty() || drawing != canvas->was_drawing;
    canvas->was_drawing = drawing;
    ink_engine::View view{editor.view(), canvas->pixel_ratio, canvas->width, canvas->height};
    if (!canvas->renderer->Update(editor.document(), view, live_changed)) return INK_OK;
    SkSurface *screen = canvas->surface->BeginFrame(canvas->width, canvas->height);
    if (!screen) return Fail(INK_ERROR_GPU, "the host surface gave no frame");
    std::optional<ink_engine::LiveInk> live;
    if (drawing) live = {editor.LivePage(), editor.LiveOutline(), editor.LivePen().color};
    canvas->renderer->Draw(screen->getCanvas(), live ? &*live : nullptr);
    canvas->surface->EndFrame();
    *drew = 1;
    return INK_OK;
  });
}

InkStatus ink_undo(InkDocument *document, int32_t *moved) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!moved) return NullArgument("moved");
    *moved = document->history.Undo();
    return INK_OK;
  });
}

InkStatus ink_redo(InkDocument *document, int32_t *moved) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!moved) return NullArgument("moved");
    *moved = document->history.Redo();
    return INK_OK;
  });
}

// ---- Layout check --------------------------------------------------------

InkStatus ink_struct_layout(InkStruct which, uint32_t *out, size_t capacity, size_t *count) {
  return Call([&] {
    if (!out || !count) return NullArgument("out");
    std::vector<size_t> layout;
    switch (which) {
      case INK_STRUCT_PEN_SAMPLE:
        layout = {sizeof(InkPenSample),
                  offsetof(InkPenSample, x),
                  offsetof(InkPenSample, y),
                  offsetof(InkPenSample, time),
                  offsetof(InkPenSample, pressure),
                  offsetof(InkPenSample, altitude),
                  offsetof(InkPenSample, azimuth),
                  offsetof(InkPenSample, roll),
                  offsetof(InkPenSample, hover_height),
                  offsetof(InkPenSample, buttons),
                  offsetof(InkPenSample, has),
                  offsetof(InkPenSample, id),
                  offsetof(InkPenSample, tool),
                  offsetof(InkPenSample, phase),
                  offsetof(InkPenSample, predicted),
                  offsetof(InkPenSample, reserved)};
        break;
      case INK_STRUCT_TOOL_SETTINGS:
        layout = {sizeof(InkToolSettings), offsetof(InkToolSettings, brush),
                  offsetof(InkToolSettings, rgb), offsetof(InkToolSettings, size)};
        break;
      case INK_STRUCT_FILE:
        layout = {sizeof(InkFile), offsetof(InkFile, path), offsetof(InkFile, bytes),
                  offsetof(InkFile, size)};
        break;
      default:
        return Fail(INK_ERROR_ARGUMENT, "unknown struct");
    }
    if (capacity < layout.size()) return Fail(INK_ERROR_ARGUMENT, "capacity too small");
    for (size_t i = 0; i < layout.size(); ++i) out[i] = uint32_t(layout[i]);
    *count = layout.size();
    return INK_OK;
  });
}

}  // extern "C"
