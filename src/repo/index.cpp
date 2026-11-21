#include "repo/index.h"

#include <fstream>

#include "core/error.h"

namespace minigit {
namespace fs = std::filesystem;

Index::Index(fs::path path) : path_(std::move(path)) {}

void Index::load() {
  entries_.clear();

  std::error_code ec;
  if (!fs::is_regular_file(path_, ec)) {
    return;
  }

  std::ifstream in(path_, std::ios::binary);
  if (!in) {
    throw MinigitError("cannot read index file: " + path_.string());
  }

  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }

    const std::size_t space = line.find(' ');
    if (space == std::string::npos) {
      throw MinigitError("corrupt index: malformed line '" + line + "'");
    }

    const Oid oid = Oid::fromHex(line.substr(0, space));
    const std::string path = line.substr(space + 1);
    if (path.empty()) {
      throw MinigitError("corrupt index: entry with empty path");
    }
    entries_[path] = oid;
  }
}

void Index::save() const {
  const fs::path temporary = path_.string() + ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) {
      throw MinigitError("cannot write index file: " + temporary.string());
    }
    for (const auto& [path, oid] : entries_) {
      out << oid.hex() << ' ' << path << '\n';
    }
    if (!out) {
      throw MinigitError("failed while writing index file: " +
                         temporary.string());
    }
  }

  std::error_code ec;
  fs::rename(temporary, path_, ec);
  if (ec) {
    throw MinigitError("cannot replace index file " + path_.string() + ": " +
                       ec.message());
  }
}

void Index::add(std::string path, const Oid& oid) {
  entries_[std::move(path)] = oid;
}

bool Index::remove(const std::string& path) {
  return entries_.erase(path) > 0;
}

std::optional<Oid> Index::oidFor(const std::string& path) const {
  const auto it = entries_.find(path);
  if (it == entries_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace minigit
