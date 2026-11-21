#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "objects/object.h"
#include "objects/oid.h"

namespace minigit {

/// A commit: one node in the history DAG.
///
/// Serialized form (text, one header per line, blank line, message):
///
///     tree <hex>
///     parent <hex>          (zero or more, in order)
///     author Name <mail> 1700000000 +0000
///     committer Name <mail> 1700000000 +0000
///
///     message
struct Commit {
  Oid tree;
  std::vector<Oid> parents;
  std::string author;
  std::int64_t timestamp = 0;
  std::string message;

  std::string serialize() const;

  /// First line of the message, used by log/status output.
  std::string summary() const;

  static Commit parse(const std::string& data);
};

}  // namespace minigit
