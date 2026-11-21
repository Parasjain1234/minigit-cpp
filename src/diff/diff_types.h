#pragma once

#include <string>
#include <vector>

namespace minigit {

enum class EditKind { Equal, Insert, Delete };

/// One step of an edit script.  `line` is the text of the line being kept,
/// inserted or deleted.
struct Edit {
  EditKind kind;
  std::string line;
};

enum class DiffAlgorithm { Lcs, Myers };

/// Rendered diff for a single tracked path.
struct FileDiff {
  std::string path;
  std::string text;
};

/// Splits text on '\n' without producing a phantom empty line for a trailing
/// newline.
std::vector<std::string> splitLines(const std::string& text);

/// Heuristic used to avoid dumping binary payloads into the terminal: a NUL
/// byte in the first 8 KiB marks the content as binary.
bool looksBinary(const std::string& content);

const char* editKindName(EditKind kind);

}  // namespace minigit
