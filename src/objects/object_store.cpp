#include "objects/object_store.h"

#include <fstream>
#include <iterator>
#include <system_error>

#include "compress/huffman.h"
#include "core/error.h"

namespace minigit {
namespace fs = std::filesystem;
namespace {

std::string readWholeFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw MinigitError("cannot open object file: " + path.string());
  }
  std::string data((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  if (in.bad()) {
    throw MinigitError("failed while reading object file: " + path.string());
  }
  return data;
}

/// Writes via a temporary file + rename so an interrupted process can never
/// leave a half written object that would later fail its hash check.
void writeAtomically(const fs::path& target, const std::string& bytes) {
  const fs::path temporary = target.string() + ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) {
      throw MinigitError("cannot create object file: " + temporary.string());
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
      throw MinigitError("failed while writing object file: " +
                         temporary.string());
    }
  }

  std::error_code ec;
  fs::rename(temporary, target, ec);
  if (ec) {
    // Another process may have written the same object in the meantime.
    fs::remove(temporary, ec);
    if (!fs::exists(target)) {
      throw MinigitError("cannot store object at " + target.string() + ": " +
                         ec.message());
    }
  }
}

}  // namespace

ObjectStore::ObjectStore(fs::path root) : root_(std::move(root)) {}

fs::path ObjectStore::pathFor(const Oid& oid) const {
  const std::string hex = oid.hex();
  return root_ / hex.substr(0, 2) / hex.substr(2);
}

bool ObjectStore::contains(const Oid& oid) const {
  std::error_code ec;
  return fs::is_regular_file(pathFor(oid), ec);
}

Oid ObjectStore::write(ObjectType type, const std::string& data) {
  const Object object(type, data);
  const Oid oid = object.oid();
  const fs::path target = pathFor(oid);
  if (contains(oid)) {
    return oid;
  }

  std::error_code ec;
  fs::create_directories(target.parent_path(), ec);
  if (ec) {
    throw MinigitError("cannot create object directory " +
                       target.parent_path().string() + ": " + ec.message());
  }

  writeAtomically(target, Huffman::compress(object.serialize()));
  return oid;
}

std::optional<Object> ObjectStore::read(const Oid& oid) const {
  const fs::path path = pathFor(oid);
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) {
    return std::nullopt;
  }

  std::string raw = readWholeFile(path);
  if (Huffman::isCompressed(raw)) {
    raw = Huffman::decompress(raw);
  }
  Object object = Object::deserialize(raw);
  if (object.oid() != oid) {
    throw MinigitError("corrupt object " + oid.hex() +
                       ": content does not match its address");
  }
  return object;
}

Object ObjectStore::readOrThrow(const Oid& oid) const {
  auto object = read(oid);
  if (!object.has_value()) {
    throw MinigitError("object " + oid.hex() + " not found");
  }
  return *object;
}

}  // namespace minigit
