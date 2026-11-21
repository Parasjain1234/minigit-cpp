#include "objects/tree.h"

#include <algorithm>

#include "core/error.h"

namespace minigit {

std::string TreeEntry::serialize() const {
  std::string out;
  out += mode;
  out += ' ';
  out += objectTypeName(type);
  out += ' ';
  out += oid.hex();
  out += ' ';
  out += name;
  out += '\n';
  return out;
}

Tree::Tree(std::vector<TreeEntry> entries) : entries_(std::move(entries)) {
  std::sort(entries_.begin(), entries_.end(),
            [](const TreeEntry& a, const TreeEntry& b) {
              return a.name < b.name;
            });
}

const TreeEntry* Tree::find(std::string_view name) const {
  for (const TreeEntry& entry : entries_) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

std::string Tree::serialize() const {
  std::string out;
  for (const TreeEntry& entry : entries_) {
    out += entry.serialize();
  }
  return out;
}

Tree Tree::parse(const std::string& data) {
  std::vector<TreeEntry> entries;
  std::size_t position = 0;

  while (position < data.size()) {
    const std::size_t newline = data.find('\n', position);
    if (newline == std::string::npos) {
      throw MinigitError("corrupt tree: unterminated entry");
    }

    const std::string line = data.substr(position, newline - position);
    position = newline + 1;

    const std::size_t firstSpace = line.find(' ');
    const std::size_t secondSpace =
        firstSpace == std::string::npos ? std::string::npos
                                        : line.find(' ', firstSpace + 1);
    const std::size_t thirdSpace =
        secondSpace == std::string::npos ? std::string::npos
                                         : line.find(' ', secondSpace + 1);
    if (thirdSpace == std::string::npos) {
      throw MinigitError("corrupt tree: malformed entry '" + line + "'");
    }

    TreeEntry entry;
    entry.mode = line.substr(0, firstSpace);
    const std::string typeName =
        line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    const auto type = objectTypeFromName(typeName);
    if (!type.has_value() || *type == ObjectType::Commit) {
      throw MinigitError("corrupt tree: invalid entry type '" + typeName +
                         "'");
    }
    entry.type = *type;
    entry.oid = Oid::fromHex(
        line.substr(secondSpace + 1, thirdSpace - secondSpace - 1));
    entry.name = line.substr(thirdSpace + 1);
    if (entry.name.empty()) {
      throw MinigitError("corrupt tree: entry with empty name");
    }

    entries.push_back(std::move(entry));
  }

  return Tree(std::move(entries));
}

}  // namespace minigit
