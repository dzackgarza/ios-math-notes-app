#include "editor/history.h"

namespace ink_engine {

DocumentHistory::DocumentHistory(Document document, uint64_t id_seed)
    : values_{std::move(document)}, ids_(id_seed) {}

void DocumentHistory::Push(Document next) {
  ++index_;
  values_ = values_.take(index_).push_back(std::move(next));
}

bool DocumentHistory::Undo() {
  if (index_ == 0) return false;
  --index_;
  return true;
}

bool DocumentHistory::Redo() {
  if (index_ + 1 == values_.size()) return false;
  ++index_;
  return true;
}

void DocumentHistory::Reset(Document document) {
  values_ = immer::vector<Document>{document};
  index_ = 0;
  saved_ = std::move(document);
}

}  // namespace ink_engine
