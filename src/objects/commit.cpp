#include "objects/commit.h"

#include <sstream>

#include "core/error.h"

namespace minigit {
namespace {

std::string firstLine(const std::string& text) {
  const std::size_t newline = text.find('\n');
  return newline == std::string::npos ? text : text.substr(0, newline);
}

}  // namespace

std::string Commit::serialize() const {
  std::ostringstream out;
  out << "tree " << tree.hex() << '\n';
  for (const Oid& parent : parents) {
    out << "parent " << parent.hex() << '\n';
  }
  const std::string identity = author.empty() ? "MiniGit <minigit@example.com>"
                                              : author;
  out << "author " << identity << ' ' << timestamp << " +0000\n";
  out << "committer " << identity << ' ' << timestamp << " +0000\n";
  out << '\n';
  out << message;
  if (!message.empty() && message.back() != '\n') {
    out << '\n';
  }
  return out.str();
}

std::string Commit::summary() const { return firstLine(message); }

Commit Commit::parse(const std::string& data) {
  const std::size_t blankLine = data.find("\n\n");
  if (blankLine == std::string::npos) {
    throw MinigitError("corrupt commit: missing header/body separator");
  }

  Commit commit;
  std::istringstream headers(data.substr(0, blankLine));
  std::string line;
  bool sawTree = false;

  while (std::getline(headers, line)) {
    if (line.rfind("tree ", 0) == 0) {
      commit.tree = Oid::fromHex(line.substr(5));
      sawTree = true;
    } else if (line.rfind("parent ", 0) == 0) {
      commit.parents.push_back(Oid::fromHex(line.substr(7)));
    } else if (line.rfind("author ", 0) == 0) {
      // "Name <mail> 1700000000 +0000" -- timestamp is the second to last
      // whitespace separated token.
      const std::string rest = line.substr(7);
      const std::size_t timezoneSpace = rest.rfind(' ');
      if (timezoneSpace == std::string::npos) {
        throw MinigitError("corrupt commit: malformed author line");
      }
      const std::string beforeTimezone = rest.substr(0, timezoneSpace);
      const std::size_t timestampSpace = beforeTimezone.rfind(' ');
      if (timestampSpace == std::string::npos) {
        throw MinigitError("corrupt commit: malformed author line");
      }
      commit.author = beforeTimezone.substr(0, timestampSpace);
      try {
        commit.timestamp = std::stoll(beforeTimezone.substr(timestampSpace + 1));
      } catch (const std::exception&) {
        throw MinigitError("corrupt commit: invalid timestamp");
      }
    }
  }

  if (!sawTree) {
    throw MinigitError("corrupt commit: missing tree header");
  }

  commit.message = data.substr(blankLine + 2);
  return commit;
}

}  // namespace minigit
