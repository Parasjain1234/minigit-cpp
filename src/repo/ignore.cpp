#include "repo/ignore.h"

#include <fstream>
#include <string>

namespace minigit {
namespace fs = std::filesystem;
namespace {

std::string trim(const std::string& text) {
  const std::size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const std::size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

/// Translates the supported glob syntax into an ECMAScript regex body.
std::string globToRegex(const std::string& glob) {
  std::string out;
  for (std::size_t i = 0; i < glob.size(); ++i) {
    const char c = glob[i];
    if (c == '*') {
      if (i + 1 < glob.size() && glob[i + 1] == '*') {
        ++i;
        if (i + 1 < glob.size() && glob[i + 1] == '/') {
          out += "(.*/)?";  // "**/" matches zero or more directories
          ++i;
        } else {
          out += ".*";
        }
      } else {
        out += "[^/]*";
      }
    } else if (c == '?') {
      out += "[^/]";
    } else if (std::string(".^$|()[]{}+\\").find(c) != std::string::npos) {
      out += '\\';
      out += c;
    } else {
      out += c;
    }
  }
  return out;
}

}  // namespace

IgnoreList::IgnoreList(fs::path workTree) : workTree_(std::move(workTree)) {}

void IgnoreList::load() {
  rules_.clear();

  std::ifstream in(workTree_ / ".minignore");
  if (!in) {
    return;
  }

  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#') {
      continue;
    }

    bool negated = false;
    if (line.front() == '!') {
      negated = true;
      line.erase(0, 1);
    }
    if (line.empty()) {
      continue;
    }

    bool directoryOnly = false;
    if (line.back() == '/') {
      directoryOnly = true;
      line.pop_back();
    }

    bool anchored = line.find('/') != std::string::npos;
    if (!line.empty() && line.front() == '/') {
      anchored = true;
      line.erase(0, 1);
    }
    if (line.empty()) {
      continue;
    }

    const std::string body = globToRegex(line);
    std::string pattern;
    if (anchored) {
      pattern = "^" + body;
    } else {
      pattern = "(^|.*/)" + body;
    }
    // A matching directory ignores its contents too.
    pattern += directoryOnly ? "(/.*)?$" : "(/.*)?$";

    try {
      rules_.emplace_back(negated, std::regex(pattern));
    } catch (const std::regex_error&) {
      // A malformed pattern is skipped rather than breaking every command.
    }
  }
}

bool IgnoreList::isIgnored(const std::string& relativePath) const {
  bool ignored = false;
  for (const auto& [negated, pattern] : rules_) {
    if (std::regex_match(relativePath, pattern)) {
      ignored = !negated;
    }
  }
  return ignored;
}

}  // namespace minigit
