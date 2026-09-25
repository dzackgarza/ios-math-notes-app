// Notebook directories on disk for tests and tools. The engine itself does no
// file I/O; hosts do this.
#pragma once

#include <string>

#include "format/notebook.h"

namespace ink_test {

// notebook.json and pages/*.svg of the notebook at `dir`.
ink_engine::NotebookFiles ReadNotebookDir(const std::string &dir);

// Writes each file under `dir`, creating directories.
void WriteNotebookFiles(const std::string &dir, const ink_engine::NotebookFiles &files);

}  // namespace ink_test
