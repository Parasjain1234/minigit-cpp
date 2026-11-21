#include "objects/object.h"

#include <cstdlib>

#include "core/error.h"

namespace minigit {

const char* objectTypeName(ObjectType type) {
  switch (type) {
    case ObjectType::Blob:
      return "blob";
    case ObjectType::Tree:
      return "tree";
    case ObjectType::Commit:
      return "commit";
  }
  return "unknown";
}

std::optional<ObjectType> objectTypeFromName(const std::string& name) {
  if (name == "blob") {
    return ObjectType::Blob;
  }
  if (name == "tree") {
    return ObjectType::Tree;
  }
  if (name == "commit") {
    return ObjectType::Commit;
  }
  return std::nullopt;
}

std::string Object::serialize() const {
  std::string out;
  out.reserve(data_.size() + 32);
  out += objectTypeName(type_);
  out += ' ';
  out += std::to_string(data_.size());
  out += '\0';
  out += data_;
  return out;
}

Oid Object::oid() const { return Oid::hashBytes(serialize()); }

Object Object::deserialize(const std::string& raw) {
  const std::size_t separator = raw.find('\0');
  if (separator == std::string::npos) {
    throw MinigitError("corrupt object: missing header separator");
  }

  const std::string header = raw.substr(0, separator);
  const std::size_t space = header.find(' ');
  if (space == std::string::npos) {
    throw MinigitError("corrupt object: malformed header '" + header + "'");
  }

  const std::string typeName = header.substr(0, space);
  const auto type = objectTypeFromName(typeName);
  if (!type.has_value()) {
    throw MinigitError("corrupt object: unknown type '" + typeName + "'");
  }

  const std::string sizeText = header.substr(space + 1);
  std::size_t consumed = 0;
  unsigned long long declaredSize = 0;
  try {
    declaredSize = std::stoull(sizeText, &consumed);
  } catch (const std::exception&) {
    throw MinigitError("corrupt object: invalid size '" + sizeText + "'");
  }
  if (consumed != sizeText.size()) {
    throw MinigitError("corrupt object: invalid size '" + sizeText + "'");
  }

  const std::string payload = raw.substr(separator + 1);
  if (payload.size() != declaredSize) {
    throw MinigitError("corrupt object: header says " + sizeText +
                       " bytes but payload has " +
                       std::to_string(payload.size()));
  }

  return Object(*type, payload);
}

}  // namespace minigit
