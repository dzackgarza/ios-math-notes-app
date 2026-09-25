#include "ink.h"

#include <algorithm>
#include <exception>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "editor/canvas.h"
#include "document/templates.h"
#include "format/notebook.h"
#include "format/page_svg.h"
#include "layout/layout.h"
#include "include/core/SkData.h"
#include "include/core/SkSurface.h"

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

InkStatus BadPageIndex() { return Fail(INK_ERROR_ARGUMENT, "page index out of range"); }

// The page a new page would be: drawn as the ghost page after the last one.
ink_engine::Page GhostPage(const InkDocument &document) {
  return ink_engine::NewPage(document.history.current(), document.template_page);
}

// One undo or redo step: `*page` is the page the step changed, or -1.
InkStatus Step(InkDocument *document, bool (ink_engine::DocumentHistory::*move)(), int32_t *moved,
               int32_t *page) {
  if (!document) return NullArgument("document");
  if (!moved || !page) return NullArgument("moved or page");
  ink_engine::Document before = document->history.current();
  *moved = (document->history.*move)();
  std::optional<size_t> changed = ink_engine::FirstChangedPage(before, document->history.current());
  *page = changed ? int32_t(*changed) : -1;
  return INK_OK;
}

ink_engine::PagePlacement GhostPlacement(const InkDocument &document,
                                         const std::vector<ink_engine::PagePlacement> &layout) {
  ink_engine::Page ghost = GhostPage(document);
  return ink_engine::GhostPlacement(layout, ghost.width, ghost.height);
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
    document->dirty_removed = ink_engine::RemovedFiles(history.current(), history.saved());
    document->dirty_view.clear();
    for (const auto &[path, bytes] : document->dirty) {
      document->dirty_view.push_back({path.c_str(), reinterpret_cast<const uint8_t *>(bytes.data()),
                                      bytes.size(), INK_FILE_WRITE});
    }
    for (const std::string &path : document->dirty_removed) {
      document->dirty_view.push_back({path.c_str(), nullptr, 0, INK_FILE_DELETE});
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
    ink_engine::PagePlacement ghost = GhostPlacement(*document, layout);
    *width = ghost.width;
    for (const auto &page : layout) *width = std::max(*width, page.width);
    *height = ghost.y + ghost.height;
    return INK_OK;
  });
}

// ---- Pages and templates -------------------------------------------------

InkStatus ink_document_page_count(InkDocument *document, size_t *count) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!count) return NullArgument("count");
    *count = ink_engine::ListedPageCount(document->history.current());
    return INK_OK;
  });
}

InkStatus ink_document_insert_page(InkDocument *document, size_t index) {
  return Call([&] {
    if (!document) return NullArgument("document");
    ink_engine::DocumentHistory &history = document->history;
    if (index > ink_engine::ListedPageCount(history.current())) return BadPageIndex();
    history.Push(ink_engine::InsertPage(history.current(), index, history.ids(),
                                        document->template_page));
    return INK_OK;
  });
}

InkStatus ink_document_delete_page(InkDocument *document, size_t index) {
  return Call([&] {
    if (!document) return NullArgument("document");
    ink_engine::DocumentHistory &history = document->history;
    if (index >= ink_engine::ListedPageCount(history.current())) return BadPageIndex();
    history.Push(ink_engine::DeletePage(history.current(), index));
    return INK_OK;
  });
}

InkStatus ink_document_move_page(InkDocument *document, size_t from, size_t to) {
  return Call([&] {
    if (!document) return NullArgument("document");
    ink_engine::DocumentHistory &history = document->history;
    size_t count = ink_engine::ListedPageCount(history.current());
    if (from >= count || to >= count) return BadPageIndex();
    if (from != to) history.Push(ink_engine::MovePage(history.current(), from, to));
    return INK_OK;
  });
}

InkStatus ink_document_set_page_size(InkDocument *document, InkPageSize size, double width,
                                     double height) {
  return Call([&] {
    if (!document) return NullArgument("document");
    ink_engine::PageSize page_size;
    switch (size) {
      case INK_PAGE_A4: page_size = std::string("A4"); break;
      case INK_PAGE_LETTER: page_size = std::string("Letter"); break;
      case INK_PAGE_CUSTOM:
        if (!(width > 0 && height > 0)) return Fail(INK_ERROR_ARGUMENT, "non-positive page size");
        page_size = std::array<double, 2>{width, height};
        break;
      default: return Fail(INK_ERROR_ARGUMENT, "unknown page size");
    }
    ink_engine::DocumentHistory &history = document->history;
    if (history.current().notebook.page_size == page_size) return INK_OK;
    history.Push(ink_engine::SetPageSize(history.current(), std::move(page_size)));
    return INK_OK;
  });
}

InkStatus ink_document_set_template(InkDocument *document, const char *name, const uint8_t *svg,
                                    size_t size) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!name) return NullArgument("name");
    if (!svg && size) return NullArgument("svg");
    ink_engine::Page page = ink_engine::ReadPage(Bytes(svg, size), "pages/0001.svg", {});
    if (page.error) return Fail(INK_ERROR_PARSE, std::string(name) + ": " + *page.error);
    document->template_page = std::move(page);
    ink_engine::DocumentHistory &history = document->history;
    if (history.current().notebook.template_name != name) {
      ink_engine::Document next = history.current();
      next.notebook.template_name = name;
      history.Push(std::move(next));
    }
    return INK_OK;
  });
}

InkStatus ink_builtin_template_count(size_t *count) {
  return Call([&] {
    if (!count) return NullArgument("count");
    *count = ink_engine::BuiltinTemplates().size();
    return INK_OK;
  });
}

InkStatus ink_builtin_template_name(size_t index, const char **name) {
  return Call([&] {
    if (!name) return NullArgument("name");
    const auto &templates = ink_engine::BuiltinTemplates();
    if (index >= templates.size()) return Fail(INK_ERROR_ARGUMENT, "no such built-in template");
    *name = templates[index].name;
    return INK_OK;
  });
}

InkStatus ink_builtin_template_create(const char *name, uint64_t seed, InkDocument **out) {
  return Call([&] {
    if (!name) return NullArgument("name");
    if (!out) return NullArgument("out");
    ink_engine::IdGenerator ids(seed);
    std::optional<ink_engine::Document> document = ink_engine::BuiltinTemplateNotebook(name, ids);
    if (!document) return Fail(INK_ERROR_ARGUMENT, std::string("no built-in template ") + name);
    *out = new InkDocument{ink_engine::DocumentHistory(std::move(*document), seed + 1)};
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

InkStatus ink_canvas_page_at(InkCanvas *canvas, double x, double y, int32_t *page) {
  return Call([&] {
    if (!canvas) return NullArgument("canvas");
    if (!page) return NullArgument("page");
    ink_engine::Point at = ink_engine::ToContent(canvas->editor.view(), x, y);
    auto contains = [&](const ink_engine::PagePlacement &p) {
      return at.x >= p.x && at.x <= p.x + p.width && at.y >= p.y && at.y <= p.y + p.height;
    };
    std::vector<ink_engine::PagePlacement> layout =
        ink_engine::LayoutPages(canvas->document->history.current());
    *page = -1;
    for (size_t i = 0; i < layout.size(); ++i) {
      if (contains(layout[i])) *page = int32_t(i);
    }
    if (contains(GhostPlacement(*canvas->document, layout))) *page = int32_t(layout.size());
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
    ink_engine::Page ghost = GhostPage(*canvas->document);
    if (!canvas->renderer->Update(editor.document(), view, live_changed, &ghost)) return INK_OK;
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

InkStatus ink_undo(InkDocument *document, int32_t *moved, int32_t *page) {
  return Call([&] { return Step(document, &ink_engine::DocumentHistory::Undo, moved, page); });
}

InkStatus ink_redo(InkDocument *document, int32_t *moved, int32_t *page) {
  return Call([&] { return Step(document, &ink_engine::DocumentHistory::Redo, moved, page); });
}

InkStatus ink_document_page_rect(InkDocument *document, size_t index, double *x, double *y,
                                 double *width, double *height) {
  return Call([&] {
    if (!document) return NullArgument("document");
    if (!x || !y || !width || !height) return NullArgument("rectangle");
    std::vector<ink_engine::PagePlacement> layout =
        ink_engine::LayoutPages(document->history.current());
    if (index >= layout.size()) return BadPageIndex();
    const ink_engine::PagePlacement &p = layout[index];
    *x = p.x, *y = p.y, *width = p.width, *height = p.height;
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
                  offsetof(InkFile, size), offsetof(InkFile, kind)};
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
