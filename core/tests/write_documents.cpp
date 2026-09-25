// Writes the expected output of each hand-made notebook in tests/documents/:
// the files the engine writes, and samples.json with each stroke's sample
// count and first and last X and Y, for core/tests/inkml_check.py.
// Usage: write_documents <documents-dir> <out-dir> <notebook>...
#include <cstdio>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "format/notebook.h"
#include "support/notebook_dir.h"

namespace {

void CollectSamples(const ink_engine::Elements &elements, const std::string &file,
                    nlohmann::ordered_json &out) {
  for (const auto &box : elements) {
    if (auto *s = std::get_if<ink_engine::Stroke>(&box->value)) {
      if (s->samples.empty()) continue;
      out.push_back({{"file", file},
                     {"id", s->id},
                     {"count", s->samples.size()},
                     {"first", {s->samples.front().x, s->samples.front().y}},
                     {"last", {s->samples.back().x, s->samples.back().y}}});
    } else if (auto *b = std::get_if<ink_engine::Bookmark>(&box->value)) {
      CollectSamples(b->children, file, out);
    } else if (auto *l = std::get_if<ink_engine::Link>(&box->value)) {
      CollectSamples(l->children, file, out);
    }
  }
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: write_documents <documents-dir> <out-dir> <notebook>...\n");
    return 2;
  }
  for (int i = 3; i < argc; ++i) {
    std::string name = argv[i];
    ink_engine::NotebookFiles written = ink_engine::AllFiles(
        ink_engine::LoadNotebook(ink_test::ReadNotebookDir(std::string(argv[1]) + "/" + name)));
    std::string out = std::string(argv[2]) + "/" + name;
    ink_test::WriteNotebookFiles(out, written);

    // Samples as the engine reads them back from its own output.
    ink_engine::Document reread = ink_engine::LoadNotebook(written);
    nlohmann::ordered_json samples = nlohmann::ordered_json::array();
    for (const auto &page : reread.pages) {
      for (const auto &layer : page->layers) CollectSamples(layer.elements, page->file, samples);
    }
    std::ofstream(out + "/samples.json") << samples.dump(2) << "\n";
  }
  return 0;
}
