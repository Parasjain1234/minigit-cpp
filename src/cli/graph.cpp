#include "cli/graph.h"

#include <algorithm>
#include <optional>

namespace minigit {
namespace {

void trimTrailingSpaces(std::string& line) {
  while (!line.empty() && line.back() == ' ') {
    line.pop_back();
  }
}

std::string connectorFor(const std::vector<std::string>& lanes,
                         std::size_t column,
                         std::optional<std::size_t> convergeAt,
                         char convergeGlyph) {
  std::string line;
  for (std::size_t i = 0; i < lanes.size(); ++i) {
    if (i == column || !lanes[i].empty()) {
      line += '|';
    } else if (convergeAt.has_value() && *convergeAt == i) {
      line += convergeGlyph;
    } else {
      line += ' ';
    }
  }
  trimTrailingSpaces(line);
  return line;
}

}  // namespace

std::vector<std::string> renderAsciiGraph(const std::vector<GraphNode>& nodes) {
  std::vector<std::string> lines;
  std::vector<std::string> lanes;  // commit id each lane waits for, "" = free

  const auto findLane = [&lanes](const std::string& id) -> std::optional<std::size_t> {
    for (std::size_t i = 0; i < lanes.size(); ++i) {
      if (lanes[i] == id) {
        return i;
      }
    }
    return std::nullopt;
  };

  const auto allocateLane = [&lanes]() -> std::size_t {
    for (std::size_t i = 0; i < lanes.size(); ++i) {
      if (lanes[i].empty()) {
        return i;
      }
    }
    lanes.emplace_back();
    return lanes.size() - 1;
  };

  for (const GraphNode& node : nodes) {
    std::optional<std::size_t> existing = findLane(node.id);
    const std::size_t column =
        existing.has_value() ? *existing : allocateLane();
    lanes[column] = node.id;

    // Row: '*' for this commit, '|' for other lanes still in flight.
    std::string row;
    for (std::size_t i = 0; i < lanes.size(); ++i) {
      if (i == column) {
        row += '*';
      } else if (!lanes[i].empty()) {
        row += '|';
      } else {
        row += ' ';
      }
    }
    trimTrailingSpaces(row);
    row += ' ';
    row += node.label;
    lines.push_back(row);

    // Attach parents.
    std::vector<std::size_t> mergeArms;

    if (node.parents.empty()) {
      lanes[column].clear();
    } else {
      const std::string& firstParent = node.parents.front();
      const std::optional<std::size_t> parentLane = findLane(firstParent);
      if (parentLane.has_value() && *parentLane != column) {
        // This branch joins a lane that is already open: close ours with a
        // "|/" connector.
        lanes[column].clear();
        std::string connector =
            connectorFor(lanes, *parentLane, column, '/');
        if (!connector.empty()) {
          lines.push_back(connector);
        }
      } else {
        lanes[column] = firstParent;
      }
    }

    for (std::size_t p = 1; p < node.parents.size(); ++p) {
      const std::string& parent = node.parents[p];
      const std::optional<std::size_t> parentLane = findLane(parent);
      if (parentLane.has_value()) {
        mergeArms.push_back(*parentLane);
        continue;
      }
      const std::size_t lane = allocateLane();
      lanes[lane] = parent;
      mergeArms.push_back(lane);
    }

    if (!mergeArms.empty()) {
      std::sort(mergeArms.begin(), mergeArms.end());
      std::string connector(lanes.size(), ' ');
      for (std::size_t i = 0; i < lanes.size(); ++i) {
        if (!lanes[i].empty() || i == column) {
          connector[i] = '|';
        }
      }
      for (std::size_t lane : mergeArms) {
        if (lane < connector.size()) {
          connector[lane] = '\\';
        }
      }
      trimTrailingSpaces(connector);
      if (!connector.empty()) {
        lines.push_back(connector);
      }
    }

    // Shrink the lane list so trailing free lanes never widen later rows.
    while (!lanes.empty() && lanes.back().empty()) {
      lanes.pop_back();
    }
  }

  return lines;
}

}  // namespace minigit
