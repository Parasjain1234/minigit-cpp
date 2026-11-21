#include "diff/myers.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "diff/lcs.h"
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

TEST(MyersTest, EmptyInputsProduceNoEdits) {
  EXPECT_TRUE(myersDiff({}, {}).empty());
}

TEST(MyersTest, InsertionsOnly) {
  const auto edits = myersDiff({}, lines({"a", "b"}));
  ASSERT_EQ(edits.size(), std::size_t{2});
  EXPECT_EQ(edits[0].kind, EditKind::Insert);
  EXPECT_EQ(edits[1].line, "b");
}

TEST(MyersTest, DeletionsOnly) {
  const auto edits = myersDiff(lines({"a", "b"}), {});
  ASSERT_EQ(edits.size(), std::size_t{2});
  EXPECT_EQ(edits[0].kind, EditKind::Delete);
}

TEST(MyersTest, IdenticalSequencesStayEqual) {
  const auto edits = myersDiff(lines({"a", "b", "c"}), lines({"a", "b", "c"}));
  ASSERT_EQ(edits.size(), std::size_t{3});
  EXPECT_EQ(changedCount(edits), std::size_t{0});
}

TEST(MyersTest, ReconstructsTheNewSequence) {
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
      {{"a"}, {"b"}},
  };

  for (const Case& testCase : cases) {
    const auto edits = myersDiff(testCase.before, testCase.after);
    EXPECT_EQ(applyEdits(edits), testCase.after);
  }
}

TEST(MyersTest, ProducesTheMinimalEditCount) {
  const auto before = lines({"a", "b", "c", "d", "e"});
  const auto after = lines({"a", "c", "d", "x", "e"});

  const std::size_t expected =
      before.size() + after.size() - 2 * lcsLength(before, after);
  EXPECT_EQ(changedCount(myersDiff(before, after)), expected);
}

TEST(MyersTest, HandlesRepeatedLines) {
  const auto before = lines({"x", "x", "y", "x"});
  const auto after = lines({"x", "y", "x", "x"});
  const auto edits = myersDiff(before, after);
  EXPECT_EQ(applyEdits(edits), after);
  EXPECT_LE(changedCount(edits), std::size_t{2});
}

TEST(MyersTest, ScrambledLargeInputStillReconstructs) {
  std::vector<std::string> before;
  std::vector<std::string> after;
  for (int i = 0; i < 60; ++i) {
    before.push_back("line " + std::to_string(i));
    if (i % 7 != 0) {
      after.push_back("line " + std::to_string(i));
    }
    if (i % 11 == 0) {
      after.push_back("added " + std::to_string(i));
    }
  }

  const auto edits = myersDiff(before, after);
  EXPECT_EQ(applyEdits(edits), after);
}
