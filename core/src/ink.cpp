#include "ink.h"

#include "editor/canvas.h"

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

}  // extern "C"
