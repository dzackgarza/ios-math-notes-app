// The object behind the C ABI's opaque InkCanvas handle.
#pragma once

#include "editor/editor.h"

namespace ink_engine {

// A new notebook: one layer and one blank A4 page (FORMAT.md).
Document NewNotebook(IdGenerator &ids);

}  // namespace ink_engine

struct InkCanvas {
  ink_engine::Editor editor;
};
