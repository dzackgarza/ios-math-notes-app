// A document being edited: its list of document values with an index (undo
// and redo move the index; docs/ARCHITECTURE.md), the value last saved, and
// the id generator its edits draw from. Follows lager doc/modularity.rst,
// history_model (arximboldi/lager ddf87b4, lines 173-289): an immer vector of
// values and a position; a new value after an undo drops the redo branch.
#pragma once

#include <cstdint>
#include <optional>
#include <immer/vector.hpp>

#include "document/document.h"
#include "document/ids.h"

namespace ink_engine {

class DocumentHistory {
 public:
  DocumentHistory(Document document, uint64_t id_seed);

  const Document &current() const { return values_[index_]; }
  // The value last saved; null for a document never saved.
  const Document *saved() const { return saved_ ? &*saved_ : nullptr; }
  size_t size() const { return values_.size(); }
  const immer::vector<Document> &values() const { return values_; }
  IdGenerator &ids() { return ids_; }

  void Push(Document next);
  bool Undo();
  bool Redo();
  // Starts over from a document read from files, which is the saved value.
  void Reset(Document document);
  void MarkSaved() { saved_ = current(); }

 private:
  immer::vector<Document> values_;
  size_t index_ = 0;
  std::optional<Document> saved_;
  IdGenerator ids_;
};

}  // namespace ink_engine
