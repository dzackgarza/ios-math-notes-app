#include "editor/history.h"

namespace ink_engine {

DocumentHistory::DocumentHistory(Document document, uint64_t id_seed) : ids_(id_seed) {
  values_.push_back(std::move(document));
}

void DocumentHistory::Push(Document next) {
  values_.resize(index_ + 1);
  values_.push_back(std::move(next));
  index_ = values_.size() - 1;
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
  values_ = {document};
  index_ = 0;
  saved_ = std::move(document);
}

}  // namespace ink_engine
