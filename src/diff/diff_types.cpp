#include "diff/diff_types.h"

#include <algorithm>

namespace minigit {

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::size_t start = 0;

  while (start < text.size()) {
    const std::size_t newline = text.find('\n', start);
    if (newline == std::string::npos) {
      lines.push_back(text.substr(start));
      break;
    }
    lines.push_back(text.substr(start, newline - start));
    start = newline + 1;
  }

  return lines;
}

bool looksBinary(const std::string& content) {
  const std::size_t limit = std::min<std::size_t>(content.size(), 8192);
  return content.find('\0', 0) != std::string::npos &&
         content.find('\0') < limit;
}

const char* editKindName(EditKind kind) {
  switch (kind) {
    case EditKind::Equal:
      return "equal";
    case EditKind::Insert:
      return "insert";
    case EditKind::Delete:
      return "delete";
  }
  return "unknown";
}

}  // namespace minigit
