#include "repo/repository.h"

#include <gtest/gtest.h>

#include <string>

#include "test_util.h"

using namespace minigit;
using minigit_test::TempDir;
using minigit_test::writeTextFile;

namespace {

struct Fixture {
  TempDir temp;
  Repository repo = Repository::init(temp.path());

  void commitFile(const std::string& name, const std::string& contents,
                  const std::string& message) {
    writeTextFile(temp.path() / name, contents);
    repo.add({temp.path() / name});
    repo.commit(message);
  }
};

}  // namespace

TEST(ExportTest, EmptyRepositoryStillProducesValidShape) {
  Fixture fixture;
  const std::string json = fixture.repo.exportJson();

  EXPECT_NE(json.find("\"branch\": \"main\""), std::string::npos);
  EXPECT_NE(json.find("\"head\": null"), std::string::npos);
  EXPECT_NE(json.find("\"commits\": ["), std::string::npos);
  EXPECT_NE(json.find("\"objectCounts\""), std::string::npos);
}

TEST(ExportTest, IncludesCommitsBranchesAndFiles) {
  Fixture fixture;
  fixture.commitFile("hello.txt", "hello\n", "first commit");
  fixture.repo.createBranch("feature");

  const Oid head = *fixture.repo.headCommit();
  const std::string json = fixture.repo.exportJson();

  EXPECT_NE(json.find("\"head\": \"" + head.hex() + "\""), std::string::npos);
  EXPECT_NE(json.find("\"id\": \"" + head.hex() + "\""), std::string::npos);
  EXPECT_NE(json.find("\"name\": \"feature\""), std::string::npos);
  EXPECT_NE(json.find("\"message\": \"first commit\""), std::string::npos);
  EXPECT_NE(json.find("\"path\": \"hello.txt\""), std::string::npos);
  EXPECT_NE(json.find("\"size\": 6"), std::string::npos);
  EXPECT_NE(json.find("\"blobs\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"commits\": 1"), std::string::npos);
}

TEST(ExportTest, EscapesQuotesAndNewlinesInMessages) {
  Fixture fixture;
  fixture.commitFile("a.txt", "one\n", "quote \" and backslash \\");
  const std::string json = fixture.repo.exportJson();
  EXPECT_NE(json.find("quote \\\" and backslash \\\\"), std::string::npos);
}
