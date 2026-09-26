// The C ABI's documents, statuses and errors (issue #4).
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

#include "editor/canvas.h"
#include "format/notebook.h"
#include "ink.h"
#include "support/notebook_dir.h"
#include "support/session.h"

using namespace ink_engine;

namespace {

const std::string kDocuments = INK_DOCUMENTS_DIR;

const uint8_t *Data(const std::string &bytes) {
  return reinterpret_cast<const uint8_t *>(bytes.data());
}

std::map<std::string, std::string> DirtyFiles(InkDocument *document) {
  const InkFile *files = nullptr;
  size_t count = 0;
  REQUIRE(ink_document_dirty_files(document, &files, &count) == INK_OK);
  std::map<std::string, std::string> out;
  for (size_t i = 0; i < count; ++i) {
    out[files[i].path] = std::string(reinterpret_cast<const char *>(files[i].bytes), files[i].size);
  }
  return out;
}

void DrawLine(InkCanvas *canvas, double y) {
  InkPenSample samples[] = {
      {.x = 100, .y = y, .time = 0, .tool = INK_TOOL_PEN, .phase = INK_PHASE_BEGIN},
      {.x = 150, .y = y, .time = 20, .id = 1, .tool = INK_TOOL_PEN, .phase = INK_PHASE_MOVE},
      {.x = 200, .y = y, .time = 40, .id = 2, .tool = INK_TOOL_PEN, .phase = INK_PHASE_END}};
  REQUIRE(ink_input(canvas, samples, 3) == INK_OK);
}

}  // namespace

TEST_CASE("A page that does not parse gives a parse status and its message") {
  ink_test::Session session;
  std::string bad = "<svg><g id=\"l-aaaaaa\">\n<<<<<<< HEAD\n</g></svg>";
  CHECK(ink_document_load_page(session.document, "pages/0001.svg", Data(bad), bad.size()) ==
        INK_ERROR_PARSE);
  CHECK(std::string(ink_last_error()) == "pages/0001.svg: Could not determine tag type at offset 24");
  // The page stays in the document as an error page, which is never written.
  const Page &page = *session.doc().pages[0];
  CHECK(page.error.has_value());
  CHECK_FALSE(DirtyFiles(session.document).contains("pages/0001.svg"));
}

TEST_CASE("notebook.json that does not parse gives a parse status and its message") {
  ink_test::Session session;
  std::string bad = "{\"title\": ";
  CHECK(ink_document_load_notebook(session.document, Data(bad), bad.size()) == INK_ERROR_PARSE);
  CHECK(std::string(ink_last_error()).starts_with("[json.exception.parse_error.101]"));
}

TEST_CASE("A null handle or pointer gives an argument status") {
  int32_t drew = 0;
  CHECK(ink_render(nullptr, &drew) == INK_ERROR_ARGUMENT);
  CHECK(std::string(ink_last_error()) == "canvas is null");
  CHECK(ink_document_create(1, nullptr) == INK_ERROR_ARGUMENT);
  ink_test::Session session;
  CHECK(ink_canvas_set_tool(session.get(), nullptr) == INK_ERROR_ARGUMENT);
  InkToolSettings unknown{7, 0, 1, 1};
  CHECK(ink_canvas_set_tool(session.get(), &unknown) == INK_ERROR_ARGUMENT);
  CHECK(ink_canvas_set_view(session.get(), 0, 0, 0, 0, 0, 0) == INK_ERROR_ARGUMENT);
}

TEST_CASE("A notebook loaded file by file equals the notebook loaded at once") {
  NotebookFiles files = ink_test::ReadNotebookDir(kDocuments + "/full");
  ink_test::Session session;
  const std::string &json = files.at("notebook.json");
  REQUIRE(ink_document_load_notebook(session.document, Data(json), json.size()) == INK_OK);
  // Pages in reverse name order: placement follows notebook.json and names.
  for (auto it = files.rbegin(); it != files.rend(); ++it) {
    if (it->first == "notebook.json") continue;
    InkStatus status =
        ink_document_load_page(session.document, it->first.c_str(), Data(it->second), it->second.size());
    CHECK(status == (it->first == "pages/0006.svg" ? INK_ERROR_PARSE : INK_OK));
  }
  CHECK(AllFiles(session.doc()) == AllFiles(LoadNotebook(files)));
  CHECK(DirtyFiles(session.document).empty());  // loaded files are saved
}

TEST_CASE("Dirty files are the changed pages until the host marks them saved") {
  ink_test::Session session;
  auto first = DirtyFiles(session.document);  // a new notebook was never saved
  CHECK(first.contains("notebook.json"));
  CHECK(first.contains("pages/0001.svg"));
  REQUIRE(ink_document_mark_saved(session.document) == INK_OK);
  CHECK(DirtyFiles(session.document).empty());

  DrawLine(session.get(), 100);
  auto changed = DirtyFiles(session.document);
  CHECK(changed.size() == 1);
  CHECK(changed.at("pages/0001.svg") == AllFiles(session.doc()).at("pages/0001.svg"));
  ink_document_mark_saved(session.document);
  CHECK(DirtyFiles(session.document).empty());
}

TEST_CASE("Undo and redo move through the document values") {
  ink_test::Session session;
  DrawLine(session.get(), 100);
  DrawLine(session.get(), 200);
  auto strokes = [&] { return session.doc().pages[0]->layers[0].elements.size(); };
  REQUIRE(strokes() == 2);
  int32_t moved = 0, page = 0;
  REQUIRE(ink_undo(session.document, &moved, &page) == INK_OK);
  CHECK((moved == 1 && strokes() == 1));
  ink_undo(session.document, &moved, &page);
  ink_undo(session.document, &moved, &page);
  CHECK((moved == 0 && strokes() == 0));
  ink_redo(session.document, &moved, &page);
  CHECK((moved == 1 && strokes() == 1));
  // A new stroke after an undo drops the redo branch.
  DrawLine(session.get(), 300);
  ink_redo(session.document, &moved, &page);
  CHECK((moved == 0 && strokes() == 2));
}
