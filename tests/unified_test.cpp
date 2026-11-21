#include "diff/unified.h"

#include <gtest/gtest.h>

#include <string>

#include "diff/diff_types.h"

using namespace minigit;

TEST(UnifiedTest, IdenticalTextProducesNothing) {
  EXPECT_EQ(renderUnifiedDiff("a/f", "b/f", "one\ntwo\n", "one\ntwo\n"), "");
}

TEST(UnifiedTest, RendersHeadersAndChangedLine) {
  const std::string diff = renderUnifiedDiff("a/f", "b/f", "a\nb\nc\n",
                                             "a\nB\nc\n");
  EXPECT_NE(diff.find("--- a/f\n"), std::string::npos);
  EXPECT_NE(diff.find("+++ b/f\n"), std::string::npos);
  EXPECT_NE(diff.find("@@ -1,3 +1,3 @@\n"), std::string::npos);
  EXPECT_NE(diff.find("-b\n"), std::string::npos);
  EXPECT_NE(diff.find("+B\n"), std::string::npos);
  EXPECT_NE(diff.find(" a\n"), std::string::npos);
}

TEST(UnifiedTest, UsesThreeLinesOfContext) {
  std::string before;
  std::string after;
  for (int i = 1; i <= 10; ++i) {
    const std::string line = "line " + std::to_string(i) + "\n";
    before += line;
    after += (i == 5) ? "changed 5\n" : line;
  }

  const std::string diff = renderUnifiedDiff("a/f", "b/f", before, after);
  EXPECT_NE(diff.find("@@ -2,7 +2,7 @@\n"), std::string::npos);
  EXPECT_EQ(diff.find("@@ -1"), std::string::npos);
}

TEST(UnifiedTest, SplitsDistantChangesIntoTwoHunks) {
  std::string before;
  std::string after;
  for (int i = 1; i <= 20; ++i) {
    const std::string line = "line " + std::to_string(i) + "\n";
    before += line;
    after += (i == 2 || i == 18) ? "changed " + std::to_string(i) + "\n" : line;
  }

  const std::string diff = renderUnifiedDiff("a/f", "b/f", before, after);
  const std::size_t first = diff.find("@@");
  const std::size_t second = diff.find("@@", first + 2);
  EXPECT_NE(first, std::string::npos);
  EXPECT_NE(second, std::string::npos);
}

TEST(UnifiedTest, RendersPureInsertion) {
  const std::string diff = renderUnifiedDiff("a/f", "b/f", "a\nc\n",
                                             "a\nb\nc\n");
  EXPECT_NE(diff.find("@@ -1,2 +1,3 @@\n"), std::string::npos);
  EXPECT_NE(diff.find("+b\n"), std::string::npos);
}

TEST(UnifiedTest, RendersDeletionToEmptyFile) {
  const std::string diff = renderUnifiedDiff("a/f", "b/f", "gone\n", "");
  EXPECT_NE(diff.find("-gone\n"), std::string::npos);
}

TEST(UnifiedTest, MyersMatchesLcsForASimpleChange) {
  const std::string lcs =
      renderUnifiedDiff("a/f", "b/f", "a\nb\nc\n", "a\nB\nc\n",
                        DiffAlgorithm::Lcs);
  const std::string myers =
      renderUnifiedDiff("a/f", "b/f", "a\nb\nc\n", "a\nB\nc\n",
                        DiffAlgorithm::Myers);
  EXPECT_EQ(lcs, myers);
}

TEST(UnifiedTest, CustomContextIsRespected) {
  std::string before;
  std::string after;
  for (int i = 1; i <= 10; ++i) {
    const std::string line = "line " + std::to_string(i) + "\n";
    before += line;
    after += (i == 5) ? "changed 5\n" : line;
  }

  const std::string diff =
      renderUnifiedDiff("a/f", "b/f", before, after, DiffAlgorithm::Lcs, 1);
  EXPECT_NE(diff.find("@@ -4,3 +4,3 @@\n"), std::string::npos);
}

TEST(BinaryDetectionTest, FlagsNulBytes) {
  EXPECT_TRUE(looksBinary(std::string("abc\0def", 7)));
  EXPECT_FALSE(looksBinary("plain text\n"));
}
