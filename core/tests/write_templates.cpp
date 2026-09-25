// Writes core/tests/fixtures/templates/<name>/: the notebook of each built-in
// template, as a host writes it to Notes/.templates/ (just template-fixtures).
// Usage: write_templates <out-dir>
#include <cstdio>
#include <string>

#include "document/templates.h"
#include "format/notebook.h"
#include "support/notebook_dir.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: write_templates <out-dir>\n");
    return 2;
  }
  for (const ink_engine::BuiltinTemplate &t : ink_engine::BuiltinTemplates()) {
    ink_engine::IdGenerator ids(1);
    ink_test::WriteNotebookFiles(std::string(argv[1]) + "/" + t.name,
                                 ink_engine::AllFiles(*ink_engine::BuiltinTemplateNotebook(t.name, ids)));
  }
  return 0;
}
