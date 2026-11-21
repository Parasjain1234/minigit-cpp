#include "diff/unified.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "diff/lcs.h"
#include "diff/myers.h"

namespace minigit {
namespace {

/// A change budget above which the O(n*m) table stops being reasonable.
constexpr std::size_t kLcsCellBudget = 4'000'000;

struct Row {
  EditKind kind;
  std::string text;
  int oldLine;  // 0 when the row does not consume an old line
  int newLine;
};

std::string formatRange(int start, int count) {
  if (count == 0) {
    return std::to_string(std::max(0, start - 1)) + ",0";
  }
  if (count == 1) {
    return std::to_string(start);
  }
  return std::to_string(start) + "," + std::to_string(count);
}

}  // namespace

std::string renderUnifiedDiff(const std::string& oldLabel,
                              const std::string& newLabel,
                              const std::string& oldText,
                              const std::string& newText,
                              DiffAlgorithm algorithm, int contextLines) {
  if (oldText == newText) {
    return "";
  }

  const std::vector<std::string> oldLines = splitLines(oldText);
  const std::vector<std::string> newLines = splitLines(newText);

  if (algorithm == DiffAlgorithm::Lcs &&
      oldLines.size() * newLines.size() > kLcsCellBudget) {
    algorithm = DiffAlgorithm::Myers;
  }

  const std::vector<Edit> edits = algorithm == DiffAlgorithm::Myers
                                      ? myersDiff(oldLines, newLines)
                                      : lcsDiff(oldLines, newLines);

  std::vector<Row> rows;
  rows.reserve(edits.size());
  int oldNumber = 1;
  int newNumber = 1;
  for (const Edit& edit : edits) {
    switch (edit.kind) {
      case EditKind::Equal:
        rows.push_back({edit.kind, edit.line, oldNumber++, newNumber++});
        break;
      case EditKind::Delete:
        rows.push_back({edit.kind, edit.line, oldNumber++, 0});
        break;
      case EditKind::Insert:
        rows.push_back({edit.kind, edit.line, 0, newNumber++});
        break;
    }
  }

  std::vector<std::size_t> changed;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].kind != EditKind::Equal) {
      changed.push_back(i);
    }
  }
  if (changed.empty()) {
    return "";
  }

  const std::size_t context = static_cast<std::size_t>(std::max(0, contextLines));
  std::vector<std::pair<std::size_t, std::size_t>> hunks;  // [begin, end)
  std::size_t begin = changed.front() > context ? changed.front() - context : 0;
  std::size_t end = std::min(rows.size(), changed.front() + context + 1);

  for (std::size_t i = 1; i < changed.size(); ++i) {
    const std::size_t nextBegin =
        changed[i] > context ? changed[i] - context : 0;
    if (nextBegin <= end + context) {
      end = std::min(rows.size(), changed[i] + context + 1);
    } else {
      hunks.emplace_back(begin, end);
      begin = nextBegin;
      end = std::min(rows.size(), changed[i] + context + 1);
    }
  }
  hunks.emplace_back(begin, end);

  std::ostringstream out;
  out << "--- " << oldLabel << '\n' << "+++ " << newLabel << '\n';

  for (const auto& [hunkBegin, hunkEnd] : hunks) {
    int oldStart = 1;
    int newStart = 1;
    for (std::size_t i = 0; i < hunkBegin; ++i) {
      if (rows[i].kind != EditKind::Insert) {
        ++oldStart;
      }
      if (rows[i].kind != EditKind::Delete) {
        ++newStart;
      }
    }

    int oldCount = 0;
    int newCount = 0;
    for (std::size_t i = hunkBegin; i < hunkEnd; ++i) {
      if (rows[i].kind != EditKind::Insert) {
        ++oldCount;
      }
      if (rows[i].kind != EditKind::Delete) {
        ++newCount;
      }
    }

    out << "@@ -" << formatRange(oldStart, oldCount) << " +"
        << formatRange(newStart, newCount) << " @@\n";

    for (std::size_t i = hunkBegin; i < hunkEnd; ++i) {
      const Row& row = rows[i];
      switch (row.kind) {
        case EditKind::Equal:
          out << ' ' << row.text << '\n';
          break;
        case EditKind::Delete:
          out << '-' << row.text << '\n';
          break;
        case EditKind::Insert:
          out << '+' << row.text << '\n';
          break;
      }
    }
  }

  return out.str();
}

}  // namespace minigit
