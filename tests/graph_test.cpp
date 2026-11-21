#include "cli/graph.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using minigit::GraphNode;
using minigit::renderAsciiGraph;

namespace {

std::string join(const std::vector<std::string>& lines) {
  std::string out;
  for (const std::string& line : lines) {
    out += line;
    out += '\n';
  }
  return out;
}

}  // namespace

TEST(GraphTest, EmptyGraphRendersNothing) {
  EXPECT_TRUE(renderAsciiGraph({}).empty());
}

TEST(GraphTest, LinearHistoryUsesOneLane) {
  const std::vector<GraphNode> nodes = {
      {"bbb", {"aaa"}, "second"},
      {"aaa", {}, "first"},
  };

  EXPECT_EQ(join(renderAsciiGraph(nodes)), "* second\n* first\n");
}

TEST(GraphTest, MergeOpensAndClosesALane) {
  const std::vector<GraphNode> nodes = {
      {"merge", {"main", "side"}, "merge"},
      {"main", {"base"}, "main work"},
      {"side", {"base"}, "side work"},
      {"base", {}, "base"},
  };

  EXPECT_EQ(join(renderAsciiGraph(nodes)),
            "* merge\n"
            "|\\\n"
            "*| main work\n"
            "|* side work\n"
            "|/\n"
            "* base\n");
}

TEST(GraphTest, SingleParentBranchesDoNotDrawArms) {
  const std::vector<GraphNode> nodes = {
      {"a", {"b"}, "a"},
      {"b", {}, "b"},
  };

  const auto lines = renderAsciiGraph(nodes);
  ASSERT_EQ(lines.size(), std::size_t{2});
  EXPECT_EQ(lines[1], "* b");
}
