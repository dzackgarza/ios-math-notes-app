// A notebook directory as bytes: notebook.json plus pages/*.svg
// (docs/FORMAT.md, Layout). The host reads and writes the files.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "document/document.h"

namespace ink_engine {

// Relative path -> bytes, e.g. "notebook.json", "pages/0001.svg".
using NotebookFiles = std::map<std::string, std::string>;

// Loads notebook.json and every pages/*.svg in `files`. Listed pages come in
// notebook.json order; files in pages/ that it does not list follow, marked
// unlisted. A page that does not parse becomes an error page.
Document LoadNotebook(const NotebookFiles &files);

// The files that differ from `saved`: every page whose box is not the object
// in `saved`, and notebook.json when its bytes differ. Error pages are never
// written. `saved` is null for a notebook that was never saved.
NotebookFiles ChangedFiles(const Document &current, const Document *saved);

// Every file of the notebook, for a first save.
NotebookFiles AllFiles(const Document &document);

std::string WriteNotebookJson(const Document &document);

// The next unused four-digit page file name, "pages/NNNN.svg".
std::string NextPageFile(const Document &document);

}  // namespace ink_engine
