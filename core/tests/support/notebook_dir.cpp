#include "support/notebook_dir.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ink_test {
namespace fs = std::filesystem;

namespace {
std::string ReadFile(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open " + path.string());
  std::ostringstream bytes;
  bytes << in.rdbuf();
  return bytes.str();
}
}  // namespace

ink_engine::NotebookFiles ReadNotebookDir(const std::string &dir) {
  ink_engine::NotebookFiles files;
  files["notebook.json"] = ReadFile(fs::path(dir) / "notebook.json");
  for (const auto &entry : fs::directory_iterator(fs::path(dir) / "pages")) {
    if (entry.path().extension() == ".svg") {
      files["pages/" + entry.path().filename().string()] = ReadFile(entry.path());
    }
  }
  return files;
}

ink_engine::NotebookFiles ReadAssets(const std::string &dir) {
  ink_engine::NotebookFiles files;
  fs::path assets = fs::path(dir) / "assets";
  if (!fs::exists(assets)) return files;
  for (const auto &entry : fs::directory_iterator(assets)) {
    files["assets/" + entry.path().filename().string()] = ReadFile(entry.path());
  }
  return files;
}

std::string ReadFile(const std::string &path) { return ReadFile(fs::path(path)); }

void WriteNotebookFiles(const std::string &dir, const ink_engine::NotebookFiles &files) {
  for (const auto &[name, bytes] : files) {
    fs::path path = fs::path(dir) / name;
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << bytes;
  }
}

}  // namespace ink_test
