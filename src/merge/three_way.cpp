#include "merge/three_way.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "diff/diff_types.h"
#include "diff/myers.h"

namespace minigit {
namespace {

struct Change {
  std::size_t baseStart = 0;
  std::size_t baseEnd = 0;
  std::vector<std::string> lines;  // replacement for base[baseStart, baseEnd)
};

/// Collapses an edit script into maximal replacement blocks.
std::vector<Change> changesFromScript(const std::vector<Edit>& edits) {
  std::vector<Change> changes;
  std::size_t baseIndex = 0;
  bool open = false;

  for (const Edit& edit : edits) {
    switch (edit.kind) {
      case EditKind::Equal:
        if (open) {
          changes.back().baseEnd = baseIndex;
          open = false;
        }
        ++baseIndex;
        break;
      case EditKind::Delete:
        if (!open) {
          changes.push_back(Change{baseIndex, baseIndex, {}});
          open = true;
        }
        ++baseIndex;
        break;
      case EditKind::Insert:
        if (!open) {
          changes.push_back(Change{baseIndex, baseIndex, {}});
          open = true;
        }
        changes.back().lines.push_back(edit.line);
        break;
    }
  }

  if (open) {
    changes.back().baseEnd = baseIndex;
  }
  return changes;
}

bool sameLines(const std::vector<std::string>& left,
               const std::vector<std::string>& right) {
  return left == right;
}

/// A change that merely rewrites the base range with identical lines is not
/// a change at all.
bool isNoOp(const Change& change, const std::vector<std::string>& baseLines) {
  if (change.baseEnd - change.baseStart != change.lines.size()) {
    return false;
  }
  for (std::size_t i = 0; i < change.lines.size(); ++i) {
    if (change.lines[i] != baseLines[change.baseStart + i]) {
      return false;
    }
  }
  return true;
}

bool rangesOverlap(const Change& left, const Change& right) {
  const bool leftEmpty = left.baseStart == left.baseEnd;
  const bool rightEmpty = right.baseStart == right.baseEnd;

  if (leftEmpty && rightEmpty) {
    return left.baseStart == right.baseStart;
  }
  if (leftEmpty) {
    return left.baseStart >= right.baseStart &&
           left.baseStart < right.baseEnd;
  }
  if (rightEmpty) {
    return right.baseStart >= left.baseStart &&
           right.baseStart < left.baseEnd;
  }
  return left.baseStart < right.baseEnd && right.baseStart < left.baseEnd;
}

void append(std::vector<std::string>& target,
            const std::vector<std::string>& lines) {
  target.insert(target.end(), lines.begin(), lines.end());
}

}  // namespace

MergeFileResult mergeText3Way(const std::string& base, const std::string& ours,
                              const std::string& theirs) {
  if (ours == theirs) {
    return {ours, false};
  }
  if (ours == base) {
    return {theirs, false};
  }
  if (theirs == base) {
    return {ours, false};
  }

  const std::vector<std::string> baseLines = splitLines(base);
  const std::vector<std::string> ourLines = splitLines(ours);
  const std::vector<std::string> theirLines = splitLines(theirs);

  std::vector<Change> ourChanges =
      changesFromScript(myersDiff(baseLines, ourLines));
  std::vector<Change> theirChanges =
      changesFromScript(myersDiff(baseLines, theirLines));

  ourChanges.erase(std::remove_if(ourChanges.begin(), ourChanges.end(),
                                  [&](const Change& change) {
                                    return isNoOp(change, baseLines);
                                  }),
                   ourChanges.end());
  theirChanges.erase(std::remove_if(theirChanges.begin(), theirChanges.end(),
                                    [&](const Change& change) {
                                      return isNoOp(change, baseLines);
                                    }),
                     theirChanges.end());

  std::vector<std::string> merged;
  std::size_t cursor = 0;
  std::size_t i = 0;
  std::size_t j = 0;
  bool conflicted = false;

  const auto copyBaseUntil = [&](std::size_t until) {
    while (cursor < until && cursor < baseLines.size()) {
      merged.push_back(baseLines[cursor]);
      ++cursor;
    }
  };

  while (i < ourChanges.size() || j < theirChanges.size()) {
    const Change* ourChange = i < ourChanges.size() ? &ourChanges[i] : nullptr;
    const Change* theirChange =
        j < theirChanges.size() ? &theirChanges[j] : nullptr;

    if (ourChange != nullptr && theirChange != nullptr &&
        rangesOverlap(*ourChange, *theirChange)) {
      const std::size_t start = std::min(ourChange->baseStart,
                                         theirChange->baseStart);
      const std::size_t end =
          std::max(ourChange->baseEnd, theirChange->baseEnd);
      copyBaseUntil(start);

      if (sameLines(ourChange->lines, theirChange->lines)) {
        append(merged, ourChange->lines);
      } else {
        conflicted = true;
        merged.push_back("<<<<<<< ours");
        append(merged, ourChange->lines);
        merged.push_back("=======");
        append(merged, theirChange->lines);
        merged.push_back(">>>>>>> theirs");
      }

      cursor = end;
      ++i;
      ++j;
    } else if (ourChange != nullptr &&
               (theirChange == nullptr ||
                ourChange->baseStart < theirChange->baseStart)) {
      copyBaseUntil(ourChange->baseStart);
      append(merged, ourChange->lines);
      cursor = ourChange->baseEnd;
      ++i;
    } else {
      copyBaseUntil(theirChange->baseStart);
      append(merged, theirChange->lines);
      cursor = theirChange->baseEnd;
      ++j;
    }
  }

  copyBaseUntil(baseLines.size());

  std::string content;
  for (std::size_t line = 0; line < merged.size(); ++line) {
    content += merged[line];
    content += '\n';
  }
  return {content, conflicted};
}

}  // namespace minigit
