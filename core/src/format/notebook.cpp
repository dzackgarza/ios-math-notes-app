#include "format/notebook.h"

#include <algorithm>
#include <cstdio>
#include <set>

#include <nlohmann/json.hpp>

#include "format/page_svg.h"

namespace ink_engine {
namespace {

using Json = nlohmann::ordered_json;

std::vector<std::string> LayerIds(const Notebook &notebook) {
  std::vector<std::string> ids;
  for (const Layer &layer : notebook.layers) ids.push_back(layer.id);
  return ids;
}

bool IsPageFile(const std::string &path) {
  return path.starts_with("pages/") && path.ends_with(".svg") &&
         path.find('/', 6) == std::string::npos;
}

Json PageSizeJson(const PageSize &size) {
  if (auto *name = std::get_if<std::string>(&size)) return *name;
  auto [w, h] = std::get<std::array<double, 2>>(size);
  return Json::array({w, h});
}

PageSize ReadPageSize(const Json &json) {
  if (json.is_array() && json.size() == 2) {
    return std::array<double, 2>{json[0].get<double>(), json[1].get<double>()};
  }
  return json.is_string() ? json.get<std::string>() : std::string("A4");
}

}  // namespace

Document LoadNotebook(const NotebookFiles &files) {
  Document document;
  std::vector<std::pair<std::string, std::string>> listed;  // id, file
  auto notebook_json = files.find("notebook.json");
  if (notebook_json != files.end()) {
    Json json = Json::parse(notebook_json->second);
    Notebook &nb = document.notebook;
    nb.title = json.value("title", "");
    nb.page_size = ReadPageSize(json.value("pageSize", Json("A4")));
    nb.template_name = json.value("template", "");
    for (const Json &layer : json.value("layers", Json::array())) {
      nb.layers.push_back({layer.value("id", ""), layer.value("name", ""),
                           layer.value("hidden", false), layer.value("locked", false)});
    }
    for (const Json &page : json.value("pages", Json::array())) {
      listed.push_back({page.value("id", ""), page.value("file", "")});
    }
  }

  std::vector<std::string> layer_ids = LayerIds(document.notebook);
  std::set<std::string> listed_files;
  for (const auto &[id, file] : listed) {
    listed_files.insert(file);
    auto bytes = files.find(file);
    Page page = bytes == files.end() ? Page{.id = id, .file = file, .error = "missing file"}
                                     : ReadPage(bytes->second, file, layer_ids);
    if (page.id.empty()) page.id = id;
    document.pages = std::move(document.pages).push_back(immer::box<Page>(std::move(page)));
  }
  for (const auto &[path, bytes] : files) {  // std::map: sorted by name
    if (!IsPageFile(path) || listed_files.contains(path)) continue;
    Page page = ReadPage(bytes, path, layer_ids);
    page.unlisted = true;
    document.pages = std::move(document.pages).push_back(immer::box<Page>(std::move(page)));
  }
  return document;
}

std::string WriteNotebookJson(const Document &document) {
  const Notebook &nb = document.notebook;
  Json json;
  json["format"] = "math-notes";
  json["version"] = 1;
  json["title"] = nb.title;
  json["pageSize"] = PageSizeJson(nb.page_size);
  json["template"] = nb.template_name;
  json["layers"] = Json::array();
  for (const Layer &layer : nb.layers) {
    json["layers"].push_back(
        {{"id", layer.id}, {"name", layer.name}, {"hidden", layer.hidden}, {"locked", layer.locked}});
  }
  json["pages"] = Json::array();
  for (const auto &page : document.pages) {
    if (page->unlisted) continue;
    json["pages"].push_back({{"id", page->id}, {"file", page->file}});
  }
  return json.dump(2) + "\n";
}

NotebookFiles ChangedFiles(const Document &current, const Document *saved) {
  NotebookFiles changed;
  std::set<const Page *> saved_pages;
  if (saved) {
    for (const auto &page : saved->pages) saved_pages.insert(&page.get());
  }
  for (const auto &page : current.pages) {
    if (page->error || saved_pages.contains(&page.get())) continue;
    changed[page->file] = WritePage(*page);
  }
  std::string json = WriteNotebookJson(current);
  if (!saved || json != WriteNotebookJson(*saved)) changed["notebook.json"] = json;
  return changed;
}

NotebookFiles AllFiles(const Document &document) { return ChangedFiles(document, nullptr); }

std::string NextPageFile(const Document &document) {
  int highest = 0;
  for (const auto &page : document.pages) {
    int number = 0;
    if (std::sscanf(page->file.c_str(), "pages/%d.svg", &number) == 1) {
      highest = std::max(highest, number);
    }
  }
  char name[32];
  std::snprintf(name, sizeof name, "pages/%04d.svg", highest + 1);
  return name;
}

}  // namespace ink_engine
