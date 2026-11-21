#include "merge/three_way.h"

#include <gtest/gtest.h>

#include <string>

using minigit::mergeText3Way;

TEST(ThreeWayTest, IdenticalSidesWinWithoutConflict) {
  const auto result = mergeText3Way("base\n", "same\n", "same\n");
  EXPECT_EQ(result.content, "same\n");
  EXPECT_FALSE(result.conflicted);
}

TEST(ThreeWayTest, UnchangedSideTakesTheOtherChange) {
  EXPECT_EQ(mergeText3Way("a\nb\n", "a\nb\n", "a\nB\n").content, "a\nB\n");
  EXPECT_EQ(mergeText3Way("a\nb\n", "a\nB\n", "a\nb\n").content, "a\nB\n");
}

TEST(ThreeWayTest, IndependentEditsMergeCleanly) {
  const std::string base = "one\ntwo\nthree\nfour\nfive\n";
  const std::string ours = "ONE\ntwo\nthree\nfour\nfive\n";
  const std::string theirs = "one\ntwo\nthree\nfour\nFIVE\n";

  const auto result = mergeText3Way(base, ours, theirs);
  EXPECT_FALSE(result.conflicted);
  EXPECT_EQ(result.content, "ONE\ntwo\nthree\nfour\nFIVE\n");
}

TEST(ThreeWayTest, SameEditAppliedTwiceIsNotAConflict) {
  const std::string base = "a\nb\nc\n";
  const std::string edited = "a\nB\nc\n";

  const auto result = mergeText3Way(base, edited, edited);
  EXPECT_FALSE(result.conflicted);
  EXPECT_EQ(result.content, edited);
}

TEST(ThreeWayTest, ConflictingEditsProduceMarkers) {
  const std::string base = "one\ntwo\nthree\n";
  const std::string ours = "one\nours\nthree\n";
  const std::string theirs = "one\ntheirs\nthree\n";

  const auto result = mergeText3Way(base, ours, theirs);
  EXPECT_TRUE(result.conflicted);
  EXPECT_NE(result.content.find("<<<<<<< ours\n"), std::string::npos);
  EXPECT_NE(result.content.find("ours\n"), std::string::npos);
  EXPECT_NE(result.content.find("=======\n"), std::string::npos);
  EXPECT_NE(result.content.find("theirs\n"), std::string::npos);
  EXPECT_NE(result.content.find(">>>>>>> theirs\n"), std::string::npos);
  EXPECT_NE(result.content.find("one\n"), std::string::npos);
  EXPECT_NE(result.content.find("three\n"), std::string::npos);
}

TEST(ThreeWayTest, DeletionAgainstModificationConflicts) {
  const std::string base = "keep\ndrop\nkeep2\n";
  const std::string ours = "keep\nkeep2\n";
  const std::string theirs = "keep\nchanged\nkeep2\n";

  const auto result = mergeText3Way(base, ours, theirs);
  EXPECT_TRUE(result.conflicted);
  EXPECT_NE(result.content.find("<<<<<<< ours\n"), std::string::npos);
  EXPECT_NE(result.content.find("changed\n"), std::string::npos);
}

TEST(ThreeWayTest, BothSidesDeletingTheSameLineIsClean) {
  const std::string base = "a\nb\nc\n";
  const std::string deleted = "a\nc\n";
  const auto result = mergeText3Way(base, deleted, deleted);
  EXPECT_FALSE(result.conflicted);
  EXPECT_EQ(result.content, deleted);
}

TEST(ThreeWayTest, InsertionsAtDifferentPlacesMerge) {
  const std::string base = "a\nb\n";
  const std::string ours = "start\na\nb\n";
  const std::string theirs = "a\nb\nend\n";

  const auto result = mergeText3Way(base, ours, theirs);
  EXPECT_FALSE(result.conflicted);
  EXPECT_EQ(result.content, "start\na\nb\nend\n");
}

TEST(ThreeWayTest, InsertionsAtTheSamePlaceConflict) {
  const std::string base = "a\nb\n";
  const std::string ours = "a\nours\nb\n";
  const std::string theirs = "a\ntheirs\nb\n";

  const auto result = mergeText3Way(base, ours, theirs);
  EXPECT_TRUE(result.conflicted);
  EXPECT_NE(result.content.find("<<<<<<< ours\n"), std::string::npos);
}

TEST(ThreeWayTest, EmptyBaseWithDifferentAdditionsConflicts) {
  const auto result = mergeText3Way("", "ours\n", "theirs\n");
  EXPECT_TRUE(result.conflicted);
  EXPECT_NE(result.content.find("ours\n"), std::string::npos);
  EXPECT_NE(result.content.find("theirs\n"), std::string::npos);
}
