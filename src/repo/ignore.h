#pragma once

#include <filesystem>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace minigit {

/// Parses `.minignore` from the repository root.
///
/// Supported syntax (a deliberate, documented subset of gitignore):
///
///   * blank lines and lines starting with '#' are comments
///   * `*` matches any run of characters except '/', `?` one character,
///     `**` any run including '/'
///   * a trailing '/' marks a directory pattern (matches the directory and
///     everything below it)
///   * a leading '/' anchors the pattern to the repository root
///   * a pattern without '/' matches a name at any depth
///   * a leading '!' re-includes a path; the last matching rule wins
class IgnoreList {
 public:
  explicit IgnoreList(std::filesystem::path workTree);

  /// Reads .minignore (a missing file means "nothing ignored").
  void load();

  /// `relativePath` uses '/' separators and is relative to the work tree.
  bool isIgnored(const std::string& relativePath) const;

  bool empty() const { return rules_.empty(); }

 private:
  std::filesystem::path workTree_;
  std::vector<std::pair<bool, std::regex>> rules_;  // (negated, pattern)
};

}  // namespace minigit
