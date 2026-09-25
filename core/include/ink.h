/* Ink engine C ABI v1. Swift and TypeScript see only this header: opaque
   handles and plain structs with fixed layouts (docs/ARCHITECTURE.md).

   Every call returns an InkStatus. On an error, ink_last_error() gives the
   message. No C++ exception crosses the ABI. */
#ifndef INK_H
#define INK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum InkStatus {
  INK_OK = 0,
  INK_ERROR_ARGUMENT = 1, /* a null handle or pointer, or a bad value */
  INK_ERROR_PARSE = 2,    /* a notebook or page file that does not parse */
  INK_ERROR_GPU = 3,      /* no GPU context or surface */
  INK_ERROR_INTERNAL = 4  /* an engine failure */
} InkStatus;

/* Engine version string, "MAJOR.MINOR.PATCH". Static storage. */
const char *ink_version(void);

/* The message of the last call that did not return INK_OK. Valid until the
   next failing call. */
const char *ink_last_error(void);

/* ---- Documents -------------------------------------------------------- */

typedef struct InkDocument InkDocument;

typedef enum InkFileKind {
  INK_FILE_WRITE = 0, /* write `bytes` to `path` */
  INK_FILE_DELETE = 1 /* delete `path` (a deleted page); `bytes` is null */
} InkFileKind;

/* One file of a notebook directory. `path` is relative to the notebook,
   e.g. "pages/0001.svg". */
typedef struct InkFile {
  const char *path;
  const uint8_t *bytes;
  size_t size;
  uint32_t kind; /* InkFileKind */
} InkFile;

/* A new notebook with one blank A4 page and one layer. `seed` seeds the id
   generator. */
InkStatus ink_document_create(uint64_t seed, InkDocument **out);
/* Replaces the document with the notebook of notebook.json. Its listed pages
   are error pages ("missing file") until ink_document_load_page loads them. */
InkStatus ink_document_load_notebook(InkDocument *document, const uint8_t *json, size_t size);
/* Loads one page file. A file that does not parse returns INK_ERROR_PARSE and
   stays in the document as an error page, which is shown and never written. */
InkStatus ink_document_load_page(InkDocument *document, const char *file, const uint8_t *svg,
                                 size_t size);
/* An image file the pages reference, e.g. "assets/p0017.png". */
InkStatus ink_document_load_asset(InkDocument *document, const char *path, const uint8_t *bytes,
                                  size_t size);
/* The files that changed since the last save. `*files` stays valid until the
   next call on the document. */
InkStatus ink_document_dirty_files(InkDocument *document, const InkFile **files, size_t *count);
/* The host wrote the dirty files. */
InkStatus ink_document_mark_saved(InkDocument *document);
/* The laid-out pages' extent in content coordinates (pt), the ghost page
   after the last page included (ink_canvas_set_view). */
InkStatus ink_document_content_size(InkDocument *document, double *width, double *height);
/* Frees the document. Free its canvases first. */
InkStatus ink_document_free(InkDocument *document);

/* ---- Pages and templates ---------------------------------------------- */

/* Listed pages, in notebook.json order. Page indices below count these. */
InkStatus ink_document_page_count(InkDocument *document, size_t *count);
/* A new page before page `index` (the count appends), in the notebook's page
   size, with the template's background. One history step each. */
InkStatus ink_document_insert_page(InkDocument *document, size_t index);
/* Removes a page; ink_document_dirty_files then lists its file for deletion. */
InkStatus ink_document_delete_page(InkDocument *document, size_t index);
/* Moves page `from` to position `to`. Page files keep their names. */
InkStatus ink_document_move_page(InkDocument *document, size_t from, size_t to);

typedef enum InkPageSize { INK_PAGE_A4 = 0, INK_PAGE_LETTER = 1, INK_PAGE_CUSTOM = 2 } InkPageSize;
/* The size of new pages: A4, Letter, or `width` × `height` pt. */
InkStatus ink_document_set_page_size(InkDocument *document, InkPageSize size, double width,
                                     double height);

/* The notebook's template: `name` under Notes/.templates/, and the bytes of
   that template notebook's pages/0001.svg, whose background new pages copy. */
InkStatus ink_document_set_template(InkDocument *document, const char *name, const uint8_t *svg,
                                    size_t size);
/* The built-in templates the host writes to Notes/.templates/ on first use. */
InkStatus ink_builtin_template_count(size_t *count);
InkStatus ink_builtin_template_name(size_t index, const char **name);
/* A document of the built-in template `name`, never saved: its dirty files
   are the template notebook's files. */
InkStatus ink_builtin_template_create(const char *name, uint64_t seed, InkDocument **out);

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

/* ---- Canvases --------------------------------------------------------- */

typedef struct InkCanvas InkCanvas;

typedef enum InkBrush {
  INK_BRUSH_PRESSURE_PEN = 0,
  INK_BRUSH_MARKER = 1,
  INK_BRUSH_HIGHLIGHTER = 2
} InkBrush;

/* The tool that new pen input uses. */
typedef struct InkToolSettings {
  uint32_t brush; /* InkBrush */
  uint32_t rgb;   /* 0xRRGGBB */
  float size;     /* pt */
} InkToolSettings;

#ifdef __EMSCRIPTEN__
/* A canvas on `document` that draws into the WebGL2 canvas element matched
   by the CSS `selector`. */
InkStatus ink_canvas_create_webgl(InkDocument *document, const char *selector, InkCanvas **out);
#endif
#ifdef __APPLE__
/* A canvas on `document` that draws into `layer`, a CAMetalLayer, with the
   host's MTLDevice and MTLCommandQueue. All three are passed unretained and
   must outlive the canvas. */
InkStatus ink_canvas_create_metal(InkDocument *document, void *device, void *queue, void *layer,
                                  InkCanvas **out);
#endif
/* The content -> view transform, as SVG matrix(a, b, c, d, e, f). Content
   coordinates are pt: the listed pages stacked top to bottom with a 9.6 pt
   gap, each centered on the widest. */
InkStatus ink_canvas_set_view(InkCanvas *canvas, double a, double b, double c, double d, double e,
                              double f);
/* The surface size in device pixels, and device pixels per view unit (CSS
   devicePixelRatio, UIKit contentScaleFactor). */
InkStatus ink_canvas_set_surface_size(InkCanvas *canvas, int32_t width, int32_t height,
                                      float pixel_ratio);
InkStatus ink_canvas_set_tool(InkCanvas *canvas, const InkToolSettings *tool);
/* UTC ms since the Unix epoch minus the host's sample clock, for mn:time. */
InkStatus ink_canvas_set_utc_offset(InkCanvas *canvas, double utc_minus_host_ms);
InkStatus ink_canvas_free(InkCanvas *canvas);
/* The page under view point (x, y): its index, the page count for the ghost
   page after the last one, or -1 for none. */
InkStatus ink_canvas_page_at(InkCanvas *canvas, double x, double y, int32_t *page);

/* One batch of samples per platform event. */
InkStatus ink_input(InkCanvas *canvas, const InkPenSample *samples, size_t count);
/* Replaces the values of earlier samples with the same `id` (UIKit estimated
   properties arrive late, sometimes after the touch ends). */
InkStatus ink_input_update(InkCanvas *canvas, const InkPenSample *samples, size_t count);

/* ---- Frame and history ------------------------------------------------ */

/* Draws a frame when the document, the view, or the live stroke changed
   since the last one. `*drew` is 1 when it drew. */
InkStatus ink_render(InkCanvas *canvas, int32_t *drew);
/* Moves the document one step back or forward in its history. `*moved` is 0
   at either end. `*page` is the page the step changed, for the host to show,
   or -1 when it changed no page (a page size or template). */
InkStatus ink_undo(InkDocument *document, int32_t *moved, int32_t *page);
InkStatus ink_redo(InkDocument *document, int32_t *moved, int32_t *page);
/* A listed page's rectangle in content coordinates (pt). */
InkStatus ink_document_page_rect(InkDocument *document, size_t index, double *x, double *y,
                                 double *width, double *height);

/* ---- Layout check ----------------------------------------------------- */

typedef enum InkStruct {
  INK_STRUCT_PEN_SAMPLE = 0,
  INK_STRUCT_TOOL_SETTINGS = 1,
  INK_STRUCT_FILE = 2
} InkStruct;

/* The struct's size, then the offset of each field in declaration order,
   into `out`. `*count` is the number of values. Wrappers in other languages
   compare their layouts with these. */
InkStatus ink_struct_layout(InkStruct which, uint32_t *out, size_t capacity, size_t *count);

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

static_assert(offsetof(InkToolSettings, brush) == 0);
static_assert(offsetof(InkToolSettings, rgb) == 4);
static_assert(offsetof(InkToolSettings, size) == 8);
static_assert(sizeof(InkToolSettings) == 12);

/* Pointer-sized fields: 4 bytes on wasm32, 8 on arm64. */
static_assert(offsetof(InkFile, path) == 0);
static_assert(offsetof(InkFile, bytes) == sizeof(void *));
static_assert(offsetof(InkFile, size) == 2 * sizeof(void *));
static_assert(offsetof(InkFile, kind) == 3 * sizeof(void *));
static_assert(sizeof(InkFile) == 4 * sizeof(void *));
#endif

#endif
