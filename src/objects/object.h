#pragma once

#include <optional>
#include <string>

#include "objects/oid.h"

namespace minigit {

/// The three object kinds stored in .minigit/objects.
enum class ObjectType { Blob, Tree, Commit };

/// Textual name used inside the object header ("blob", "tree", "commit").
const char* objectTypeName(ObjectType type);

std::optional<ObjectType> objectTypeFromName(const std::string& name);

/// A single stored object: its type plus its uncompressed payload.
///
/// On disk every object is encoded as
///
///     <type> <size>\0<payload>
///
/// and the object id is the SHA-1 of those exact bytes.  This is what makes
/// the store content-addressable: identical payloads collapse to one file,
/// and any change to the payload changes every id above it in the tree.
class Object {
 public:
  Object(ObjectType type, std::string data)
      : type_(type), data_(std::move(data)) {}

  ObjectType type() const { return type_; }
  const std::string& data() const { return data_; }

  /// Header + payload, the bytes that are hashed and written to disk.
  std::string serialize() const;

  /// SHA-1 of serialize().
  Oid oid() const;

  /// Inverse of serialize().  Throws MinigitError on malformed input.
  static Object deserialize(const std::string& raw);

 private:
  ObjectType type_;
  std::string data_;
};

}  // namespace minigit
