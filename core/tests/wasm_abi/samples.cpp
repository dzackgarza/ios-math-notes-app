// Test exports for hosts/web/src/engine/engine.test.ts: the engine side of
// the InkPenSample round trip through the TypeScript wrapper.
#include <emscripten/emscripten.h>

#include <vector>

#include "ink.h"

namespace {
std::vector<InkPenSample> gStored;
}

extern "C" {

// Appends `count` samples.
EMSCRIPTEN_KEEPALIVE void test_store_samples(const InkPenSample *samples, size_t count) {
  gStored.insert(gStored.end(), samples, samples + count);
}

// Writes the stored samples to `out` field by field, into zeroed records, so
// a field the wrapper wrote at another offset does not come back where the
// wrapper reads it. Returns how many.
EMSCRIPTEN_KEEPALIVE size_t test_load_samples(InkPenSample *out) {
  for (size_t i = 0; i < gStored.size(); ++i) {
    const InkPenSample &s = gStored[i];
    out[i] = InkPenSample{.x = s.x, .y = s.y, .time = s.time, .pressure = s.pressure,
                          .altitude = s.altitude, .azimuth = s.azimuth, .roll = s.roll,
                          .hover_height = s.hover_height, .buttons = s.buttons, .has = s.has,
                          .id = s.id, .tool = s.tool, .phase = s.phase,
                          .predicted = s.predicted};
  }
  return gStored.size();
}

}  // extern "C"
