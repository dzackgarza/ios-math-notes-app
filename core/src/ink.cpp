#include "ink.h"

#include <optional>

#include "editor/canvas.h"
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

extern "C" {

const char *ink_version(void) { return INK_VERSION; }

InkCanvas *ink_canvas_create(uint64_t seed) {
  ink_engine::IdGenerator ids(seed);
  ink_engine::Document document = ink_engine::NewNotebook(ids);
  return new InkCanvas{ink_engine::Editor(std::move(document), seed + 1)};
}

void ink_canvas_destroy(InkCanvas *canvas) { delete canvas; }

void ink_canvas_set_view(InkCanvas *canvas, double a, double b, double c, double d, double e,
                         double f) {
  canvas->editor.SetView({a, b, c, d, e, f});
}

void ink_canvas_set_pen(InkCanvas *canvas, InkBrush brush, uint32_t rgb, float size) {
  canvas->editor.SetPen({.brush = brush,
                         .color = {uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb)},
                         .size = size});
}

void ink_canvas_set_utc_offset(InkCanvas *canvas, double utc_minus_host_ms) {
  canvas->editor.SetUtcOffset(utc_minus_host_ms);
}

void ink_input(InkCanvas *canvas, const InkPenSample *samples, size_t count) {
  canvas->editor.Input(samples, count);
}

void ink_input_update(InkCanvas *canvas, const InkPenSample *samples, size_t count) {
  canvas->editor.InputUpdate(samples, count);
}

static int Attach(InkCanvas *canvas, std::unique_ptr<ink_engine::HostSurface> surface) {
  if (!surface) return 1;
  canvas->renderer.reset();
  canvas->surface = std::move(surface);
  canvas->renderer = std::make_unique<ink_engine::Renderer>(canvas->surface->context());
  return 0;
}

#ifdef __EMSCRIPTEN__
int ink_canvas_attach_webgl(InkCanvas *canvas, const char *selector) {
  return Attach(canvas, ink_engine::MakeWebGLSurface(selector));
}
#endif

#ifdef __APPLE__
int ink_canvas_attach_metal(InkCanvas *canvas, void *ca_metal_layer) {
  return Attach(canvas, ink_engine::MakeMetalSurface(ca_metal_layer));
}
#endif

void ink_canvas_set_surface_size(InkCanvas *canvas, int width, int height, float pixel_ratio) {
  canvas->width = width;
  canvas->height = height;
  canvas->pixel_ratio = pixel_ratio;
}

int ink_render(InkCanvas *canvas) {
  if (!canvas->renderer || canvas->width <= 0 || canvas->height <= 0) return 0;
  ink_engine::Editor &editor = canvas->editor;
  bool drawing = editor.Drawing();
  bool live_changed = !editor.TakeUpdatedRegion().IsEmpty() || drawing != canvas->was_drawing;
  canvas->was_drawing = drawing;
  ink_engine::View view{editor.view(), canvas->pixel_ratio, canvas->width, canvas->height};
  if (!canvas->renderer->Update(editor.document(), view, live_changed)) return 0;
  SkSurface *screen = canvas->surface->BeginFrame(canvas->width, canvas->height);
  if (!screen) return 0;
  std::optional<ink_engine::LiveInk> live;
  if (drawing) live = {editor.LivePage(), editor.LiveOutline(), editor.LivePen().color};
  canvas->renderer->Draw(screen->getCanvas(), live ? &*live : nullptr);
  canvas->surface->EndFrame();
  return 1;
}

}  // extern "C"
