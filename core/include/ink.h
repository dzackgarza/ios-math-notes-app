/* Ink engine C ABI. Swift and TypeScript see only this header. */
#ifndef INK_H
#define INK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Engine version string, "MAJOR.MINOR.PATCH". Static storage. */
const char *ink_version(void);

#ifdef __APPLE__
/* Draws one test frame (a red triangle on white) into a CAMetalLayer and
   presents it. `ca_metal_layer` is a CAMetalLayer, passed unretained; its
   drawableSize must already be set. Returns 0 on success. */
int ink_metal_draw_test_frame(void *ca_metal_layer);
#endif

#ifdef __cplusplus
}
#endif

#endif
