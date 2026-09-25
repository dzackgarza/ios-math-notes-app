// Notebook directories on disk for tests and tools. The engine itself does no
// file I/O; hosts do this.
#pragma once

#include <string>

#include "format/notebook.h"

namespace ink_test {

// notebook.json and pages/*.svg of the notebook at `dir`.
ink_engine::NotebookFiles ReadNotebookDir(const std::string &dir);

// assets/* of the notebook at `dir`, keyed "assets/<name>"; empty without assets/.
ink_engine::NotebookFiles ReadAssets(const std::string &dir);

// The bytes of one file.
std::string ReadFile(const std::string &path);

// Writes each file under `dir`, creating directories.
void WriteNotebookFiles(const std::string &dir, const ink_engine::NotebookFiles &files);

}  // namespace ink_test
