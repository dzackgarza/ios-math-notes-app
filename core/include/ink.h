/* Ink engine C ABI. Swift and TypeScript see only this header: opaque
   handles and plain structs with fixed layouts (docs/ARCHITECTURE.md). */
#ifndef INK_H
#define INK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Engine version string, "MAJOR.MINOR.PATCH". Static storage. */
const char *ink_version(void);

/* ---- Input ------------------------------------------------------------ */

typedef enum InkTool { INK_TOOL_PEN = 0, INK_TOOL_ERASER = 1, INK_TOOL_TOUCH = 2, INK_TOOL_MOUSE = 3 } InkTool;

typedef enum InkPhase {
  INK_PHASE_HOVER = 0,
  INK_PHASE_BEGIN = 1,
  INK_PHASE_MOVE = 2,
  INK_PHASE_END = 3,
  INK_PHASE_CANCEL = 4
} InkPhase;

/* Capability bits: set when the platform measures the value. */
enum {
  INK_HAS_PRESSURE = 1u << 0,
  INK_HAS_ALTITUDE = 1u << 1,
  INK_HAS_AZIMUTH = 1u << 2,
  INK_HAS_ROLL = 1u << 3,
  INK_HAS_HOVER_HEIGHT = 1u << 4
};

typedef struct InkPenSample {
  double x, y;        /* view coordinates: CSS px, UIKit points */
  double time;        /* ms, monotonic clock of the host */
  float pressure;     /* 0..1 */
  float altitude;     /* rad, 0 = parallel to the screen */
  float azimuth;      /* rad */
  float roll;         /* rad: Pencil Pro rollAngle, web twist */
  float hover_height; /* 0..1, UIKit zOffset; iPad only */
  uint32_t buttons;   /* web buttons bits; 32 = eraser */
  uint32_t has;       /* INK_HAS_* capability bits */
  uint32_t id;        /* host sample id, for later updates */
  uint8_t tool;       /* InkTool */
  uint8_t phase;      /* InkPhase */
  uint8_t predicted;  /* 1 for a predicted sample */
  uint8_t reserved;
} InkPenSample;

/* ---- Canvas ----------------------------------------------------------- */

typedef struct InkCanvas InkCanvas;

typedef enum InkBrush {
  INK_BRUSH_PRESSURE_PEN = 0,
  INK_BRUSH_MARKER = 1,
  INK_BRUSH_HIGHLIGHTER = 2
} InkBrush;

/* A canvas on a new notebook with one A4 page and one layer. `seed` seeds
   the id generator. */
InkCanvas *ink_canvas_create(uint64_t seed);
void ink_canvas_destroy(InkCanvas *canvas);

/* The content -> view transform, as SVG matrix(a, b, c, d, e, f). Content
   coordinates are pt: the listed pages stacked top to bottom with a 9.6 pt
   gap, each centered on the widest. */
void ink_canvas_set_view(InkCanvas *canvas, double a, double b, double c, double d, double e,
                         double f);
/* Pen for new strokes. `rgb` is 0xRRGGBB, `size` in pt. */
void ink_canvas_set_pen(InkCanvas *canvas, InkBrush brush, uint32_t rgb, float size);
/* UTC ms since the Unix epoch minus the host's sample clock, for mn:time. */
void ink_canvas_set_utc_offset(InkCanvas *canvas, double utc_minus_host_ms);

/* One batch of samples per platform event. */
void ink_input(InkCanvas *canvas, const InkPenSample *samples, size_t count);
/* Replaces the values of earlier samples with the same `id` (UIKit estimated
   properties arrive late, sometimes after the touch ends). */
void ink_input_update(InkCanvas *canvas, const InkPenSample *samples, size_t count);

/* ---- Rendering -------------------------------------------------------- */

#ifdef __EMSCRIPTEN__
/* Draws into the WebGL2 canvas matched by the CSS `selector`. Returns 0 on
   success. */
int ink_canvas_attach_webgl(InkCanvas *canvas, const char *selector);
#endif
#ifdef __APPLE__
/* Draws into `ca_metal_layer`, a CAMetalLayer passed unretained, which must
   outlive the canvas. Returns 0 on success. */
int ink_canvas_attach_metal(InkCanvas *canvas, void *ca_metal_layer);
#endif
/* The surface size in device pixels, and device pixels per view unit (CSS
   devicePixelRatio, UIKit contentScaleFactor). */
void ink_canvas_set_surface_size(InkCanvas *canvas, int width, int height, float pixel_ratio);
/* Draws a frame when the document, the view, or the live stroke changed
   since the last one. Returns 1 when it drew. */
int ink_render(InkCanvas *canvas);

#ifdef __cplusplus
}

static_assert(offsetof(InkPenSample, x) == 0);
static_assert(offsetof(InkPenSample, y) == 8);
static_assert(offsetof(InkPenSample, time) == 16);
static_assert(offsetof(InkPenSample, pressure) == 24);
static_assert(offsetof(InkPenSample, altitude) == 28);
static_assert(offsetof(InkPenSample, azimuth) == 32);
static_assert(offsetof(InkPenSample, roll) == 36);
static_assert(offsetof(InkPenSample, hover_height) == 40);
static_assert(offsetof(InkPenSample, buttons) == 44);
static_assert(offsetof(InkPenSample, has) == 48);
static_assert(offsetof(InkPenSample, id) == 52);
static_assert(offsetof(InkPenSample, tool) == 56);
static_assert(offsetof(InkPenSample, phase) == 57);
static_assert(offsetof(InkPenSample, predicted) == 58);
static_assert(offsetof(InkPenSample, reserved) == 59);
static_assert(sizeof(InkPenSample) == 64);
#endif

#endif
