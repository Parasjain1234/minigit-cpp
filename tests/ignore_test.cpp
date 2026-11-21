#include "repo/ignore.h"

#include <gtest/gtest.h>

#include "test_util.h"

using minigit::IgnoreList;
using minigit_test::TempDir;
using minigit_test::writeTextFile;

namespace {

struct Fixture {
  TempDir temp;
  IgnoreList list = IgnoreList(temp.path());

  void rules(const std::string& text) {
    writeTextFile(temp.path() / ".minignore", text);
    list.load();
  }
};

}  // namespace

TEST(IgnoreTest, MissingFileIgnoresNothing) {
  Fixture fixture;
  fixture.list.load();
  EXPECT_TRUE(fixture.list.empty());
  EXPECT_FALSE(fixture.list.isIgnored("anything.txt"));
}

TEST(IgnoreTest, SkipsCommentsAndBlankLines) {
  Fixture fixture;
  fixture.rules("# comment\n\n   \n*.log\n");
  EXPECT_TRUE(fixture.list.isIgnored("debug.log"));
  EXPECT_FALSE(fixture.list.isIgnored("comment"));
}

TEST(IgnoreTest, StarMatchesBasenameAtAnyDepth) {
  Fixture fixture;
  fixture.rules("*.log\n");
  EXPECT_TRUE(fixture.list.isIgnored("debug.log"));
  EXPECT_TRUE(fixture.list.isIgnored("logs/nested/debug.log"));
  EXPECT_FALSE(fixture.list.isIgnored("debug.txt"));
  EXPECT_FALSE(fixture.list.isIgnored("debug.log.bak"));
}

TEST(IgnoreTest, DirectoryPatternIgnoresContents) {
  Fixture fixture;
  fixture.rules("build/\n");
  EXPECT_TRUE(fixture.list.isIgnored("build"));
  EXPECT_TRUE(fixture.list.isIgnored("build/object.o"));
  EXPECT_TRUE(fixture.list.isIgnored("src/build/object.o"));
  EXPECT_FALSE(fixture.list.isIgnored("builds/object.o"));
}

TEST(IgnoreTest, LeadingSlashAnchorsToRoot) {
  Fixture fixture;
  fixture.rules("/root-only.txt\n");
  EXPECT_TRUE(fixture.list.isIgnored("root-only.txt"));
  EXPECT_FALSE(fixture.list.isIgnored("sub/root-only.txt"));
}

TEST(IgnoreTest, PatternsWithSlashAreAnchored) {
  Fixture fixture;
  fixture.rules("docs/*.tmp\n");
  EXPECT_TRUE(fixture.list.isIgnored("docs/draft.tmp"));
  EXPECT_FALSE(fixture.list.isIgnored("other/docs/draft.tmp"));
  EXPECT_FALSE(fixture.list.isIgnored("docs/draft.txt"));
}

TEST(IgnoreTest, NegationReincludesAndLastRuleWins) {
  Fixture fixture;
  fixture.rules("*.log\n!important.log\n");
  EXPECT_TRUE(fixture.list.isIgnored("debug.log"));
  EXPECT_FALSE(fixture.list.isIgnored("important.log"));
  EXPECT_FALSE(fixture.list.isIgnored("nested/important.log"));
}

TEST(IgnoreTest, DoubleStarMatchesAcrossDirectories) {
  Fixture fixture;
  fixture.rules("logs/**/debug.log\n");
  EXPECT_TRUE(fixture.list.isIgnored("logs/debug.log"));
  EXPECT_TRUE(fixture.list.isIgnored("logs/a/b/debug.log"));
  EXPECT_FALSE(fixture.list.isIgnored("debug.log"));
}

TEST(IgnoreTest, QuestionMarkMatchesOneCharacter) {
  Fixture fixture;
  fixture.rules("file?.txt\n");
  EXPECT_TRUE(fixture.list.isIgnored("file1.txt"));
  EXPECT_FALSE(fixture.list.isIgnored("file10.txt"));
}
