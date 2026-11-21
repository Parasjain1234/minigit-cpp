#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "objects/object.h"
#include "objects/oid.h"

namespace minigit {

/// Content-addressable store rooted at .minigit/objects.
///
/// Objects are sharded on the first two hex characters of their id:
///
///     .minigit/objects/ab/cdef...   (remaining 38 characters)
///
/// which keeps directory listings small even for repositories with many
/// thousands of objects.
class ObjectStore {
 public:
  explicit ObjectStore(std::filesystem::path root);

  /// Writes an object and returns its id.  Writing the same payload twice is
  /// a no-op returning the same id, so history sharing is automatic.
  Oid write(ObjectType type, const std::string& data);

  bool contains(const Oid& oid) const;

  /// Returns std::nullopt when the object is absent.
  std::optional<Object> read(const Oid& oid) const;

  /// Reads an object or throws a descriptive MinigitError.
  Object readOrThrow(const Oid& oid) const;

  std::filesystem::path pathFor(const Oid& oid) const;

  const std::filesystem::path& root() const { return root_; }

 private:
  std::filesystem::path root_;
};

}  // namespace minigit
