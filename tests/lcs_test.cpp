#include "diff/lcs.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "test_util.h"

using namespace minigit;
using minigit_test::applyEdits;

namespace {

std::vector<std::string> lines(std::initializer_list<const char*> values) {
  return std::vector<std::string>(values.begin(), values.end());
}

std::size_t changedCount(const std::vector<Edit>& edits) {
  std::size_t count = 0;
  for (const Edit& edit : edits) {
    if (edit.kind != EditKind::Equal) {
      ++count;
    }
  }
  return count;
}

}  // namespace

TEST(LcsTest, IdenticalSequencesAreAllEqual) {
  const auto edits = lcsDiff(lines({"a", "b", "c"}), lines({"a", "b", "c"}));
  ASSERT_EQ(edits.size(), std::size_t{3});
  for (const Edit& edit : edits) {
    EXPECT_EQ(edit.kind, EditKind::Equal);
  }
}

TEST(LcsTest, DetectsInsertion) {
  const auto edits = lcsDiff(lines({"a", "c"}), lines({"a", "b", "c"}));
  EXPECT_EQ(changedCount(edits), std::size_t{1});
  ASSERT_EQ(edits.size(), std::size_t{3});
  EXPECT_EQ(edits[1].kind, EditKind::Insert);
  EXPECT_EQ(edits[1].line, "b");
}

TEST(LcsTest, DetectsDeletion) {
  const auto edits = lcsDiff(lines({"a", "b", "c"}), lines({"a", "c"}));
  EXPECT_EQ(changedCount(edits), std::size_t{1});
  ASSERT_EQ(edits.size(), std::size_t{3});
  EXPECT_EQ(edits[1].kind, EditKind::Delete);
  EXPECT_EQ(edits[1].line, "b");
}

TEST(LcsTest, HandlesEmptyInputs) {
  EXPECT_TRUE(lcsDiff({}, {}).empty());
  const auto onlyInserts = lcsDiff({}, lines({"x", "y"}));
  ASSERT_EQ(onlyInserts.size(), std::size_t{2});
  EXPECT_EQ(onlyInserts[0].kind, EditKind::Insert);
  const auto onlyDeletes = lcsDiff(lines({"x", "y"}), {});
  ASSERT_EQ(onlyDeletes.size(), std::size_t{2});
  EXPECT_EQ(onlyDeletes[1].kind, EditKind::Delete);
}

TEST(LcsTest, ReconstructsTheNewSequence) {
  struct Case {
    std::vector<std::string> before;
    std::vector<std::string> after;
  };
  const std::vector<Case> cases = {
      {{"a", "b", "c"}, {"a", "b", "c"}},
      {{"a", "b", "c"}, {"a", "x", "b", "c", "y"}},
      {{"one", "two", "three", "four"}, {"one", "three", "four"}},
      {{"a", "b", "c", "d"}, {"x", "b", "y", "d", "z"}},
      {{}, {"only", "new"}},
      {{"only", "old"}, {}},
  };

  for (const Case& testCase : cases) {
    const auto edits = lcsDiff(testCase.before, testCase.after);
    EXPECT_EQ(applyEdits(edits), testCase.after);
  }
}

TEST(LcsTest, ComputesKnownSubsequenceLengths) {
  EXPECT_EQ(lcsLength({}, {}), std::size_t{0});
  EXPECT_EQ(lcsLength(lines({"a"}), {}), std::size_t{0});
  EXPECT_EQ(lcsLength(lines({"a", "b", "c"}), lines({"a", "c"})),
            std::size_t{2});
  EXPECT_EQ(lcsLength(lines({"a", "b", "c"}), lines({"d", "e"})),
            std::size_t{0});
  EXPECT_EQ(lcsLength(lines({"x", "a", "y", "b"}), lines({"a", "b"})),
            std::size_t{2});
}

TEST(LcsTest, EditCountMatchesTheLcsLength) {
  const auto before = lines({"a", "b", "c", "d", "e"});
  const auto after = lines({"a", "c", "d", "x", "e"});
  const auto edits = lcsDiff(before, after);

  const std::size_t expected =
      before.size() + after.size() - 2 * lcsLength(before, after);
  EXPECT_EQ(changedCount(edits), expected);
}
