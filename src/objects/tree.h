#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "objects/object.h"
#include "objects/oid.h"

namespace minigit {

/// File modes understood by MiniGit.  Regular files and directories only;
/// executable bits and symlinks are future work.
inline constexpr const char* kFileMode = "100644";
inline constexpr const char* kTreeMode = "40000";

/// One line of a tree object.
struct TreeEntry {
  std::string mode;
  ObjectType type;
  Oid oid;
  std::string name;

  std::string serialize() const;
};

/// A directory listing: the node type of the Merkle tree.
///
/// Entries are serialized one per line as
///
///     100644 blob <hex> <name>
///     40000 tree <hex> <name>
///
/// and are always sorted by name, so the same directory content always
/// produces byte-identical tree objects (and therefore the same object id).
class Tree {
 public:
  explicit Tree(std::vector<TreeEntry> entries);

  const std::vector<TreeEntry>& entries() const { return entries_; }
  const TreeEntry* find(std::string_view name) const;

  std::string serialize() const;

  /// Parses the payload of a tree object.  Throws MinigitError on damage.
  static Tree parse(const std::string& data);

 private:
  std::vector<TreeEntry> entries_;
};

}  // namespace minigit
