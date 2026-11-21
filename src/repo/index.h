#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "objects/oid.h"

namespace minigit {

/// The staging area, stored as text at .minigit/index:
///
///     <object-id> <repository-relative-path>
///
/// A std::map keeps entries sorted, which makes tree construction and file
/// output deterministic.  The index is the "proposed next tree"; `add`
/// rewrites individual entries, `commit` turns the whole map into trees.
class Index {
 public:
  explicit Index(std::filesystem::path path);

  /// Loads the index from disk.  A missing file is treated as empty.
  void load();

  /// Writes the index back atomically.
  void save() const;

  void add(std::string path, const Oid& oid);
  bool remove(const std::string& path);
  void clear() { entries_.clear(); }

  std::optional<Oid> oidFor(const std::string& path) const;

  const std::map<std::string, Oid>& entries() const { return entries_; }
  bool empty() const { return entries_.empty(); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
  std::map<std::string, Oid> entries_;
};

}  // namespace minigit
