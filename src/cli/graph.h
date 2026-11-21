#pragma once

#include <string>
#include <vector>

namespace minigit {

/// One row of `minigit log --graph`.
struct GraphNode {
  std::string id;                 // full commit id
  std::vector<std::string> parents;
  std::string label;              // text printed after the graph columns
};

/// Renders a commit DAG as ASCII lanes.
///
/// The renderer keeps a vector of "lanes", each holding the id of the commit
/// that lane is still waiting for.  Walking the nodes newest-first:
///
///   * a lane is opened for a commit that is not tracked yet,
///   * the first parent continues in the same lane,
///   * additional parents (a merge) open new lanes, drawn as a backslash
///     arm,
///   * a lane that reaches a commit already drawn elsewhere is collapsed
///     with a slash connector.
///
/// Output for a merge of two branches:
///
///     * merge
///     |\    (merge arms)
///     *| main work
///     |* side work
///     |/
///     * base
///
/// (The diagram is illustrative; run `minigit log --graph` to see it live.)
std::vector<std::string> renderAsciiGraph(const std::vector<GraphNode>& nodes);

}  // namespace minigit
