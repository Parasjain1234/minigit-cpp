#include "merge/lca.h"

#include <gtest/gtest.h>

#include "objects/object.h"
#include "test_util.h"

using namespace minigit;
using minigit_test::TempDir;
using minigit_test::writeTestCommit;

namespace {

struct Dag {
  TempDir temp;
  ObjectStore store = ObjectStore(temp.path());
  Oid tree = store.write(ObjectType::Tree, "");

  Oid commit(std::int64_t timestamp, const std::vector<Oid>& parents,
             const std::string& message) {
    return writeTestCommit(store, tree, parents, timestamp, message);
  }
};

}  // namespace

TEST(LcaTest, SameCommitIsItsOwnBase) {
  Dag dag;
  const Oid a = dag.commit(1, {}, "a");
  EXPECT_EQ(findMergeBase(dag.store, a, a), a);
}

TEST(LcaTest, SiblingsShareTheirParent) {
  Dag dag;
  const Oid base = dag.commit(1, {}, "base");
  const Oid left = dag.commit(2, {base}, "left");
  const Oid right = dag.commit(3, {base}, "right");

  EXPECT_EQ(findMergeBase(dag.store, left, right), base);
  EXPECT_EQ(findMergeBase(dag.store, right, left), base);
}

TEST(LcaTest, PicksTheDeepestSharedAncestor) {
  Dag dag;
  const Oid root = dag.commit(1, {}, "root");
  const Oid middle = dag.commit(2, {root}, "middle");
  const Oid left = dag.commit(3, {middle}, "left");
  const Oid right = dag.commit(4, {middle}, "right");
  const Oid leftTip = dag.commit(5, {left}, "left tip");

  EXPECT_EQ(findMergeBase(dag.store, leftTip, right), middle);
}

TEST(LcaTest, AncestorOfTheOtherIsTheBase) {
  Dag dag;
  const Oid first = dag.commit(1, {}, "first");
  const Oid second = dag.commit(2, {first}, "second");
  const Oid third = dag.commit(3, {second}, "third");

  EXPECT_EQ(findMergeBase(dag.store, first, third), first);
  EXPECT_EQ(findMergeBase(dag.store, third, first), first);
}

TEST(LcaTest, MergeCommitBaseWithBothSides) {
  Dag dag;
  const Oid base = dag.commit(1, {}, "base");
  const Oid left = dag.commit(2, {base}, "left");
  const Oid right = dag.commit(3, {base}, "right");
  const Oid merge = dag.commit(4, {left, right}, "merge");

  EXPECT_EQ(findMergeBase(dag.store, merge, left), left);
  EXPECT_EQ(findMergeBase(dag.store, merge, right), right);
}

TEST(LcaTest, UnrelatedHistoriesHaveNoBase) {
  Dag dag;
  const Oid first = dag.commit(1, {}, "first");
  const Oid second = dag.commit(2, {}, "second");

  EXPECT_EQ(findMergeBase(dag.store, first, second), std::nullopt);
  EXPECT_FALSE(isAncestor(dag.store, first, second));
}

TEST(LcaTest, DetectsAncestry) {
  Dag dag;
  const Oid first = dag.commit(1, {}, "first");
  const Oid second = dag.commit(2, {first}, "second");
  const Oid branch = dag.commit(3, {first}, "branch");

  EXPECT_TRUE(isAncestor(dag.store, first, second));
  EXPECT_TRUE(isAncestor(dag.store, second, second));
  EXPECT_FALSE(isAncestor(dag.store, second, first));
  EXPECT_FALSE(isAncestor(dag.store, branch, second));
}
