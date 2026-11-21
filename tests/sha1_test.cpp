#include "hash/sha1.h"

#include <gtest/gtest.h>

#include <string>

using minigit::Sha1;

TEST(Sha1Test, EmptyString) {
  EXPECT_EQ(Sha1::hash(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(Sha1Test, Abc) {
  EXPECT_EQ(Sha1::hash("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(Sha1Test, TwoBlockMessage) {
  EXPECT_EQ(Sha1::hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

TEST(Sha1Test, QuickBrownFox) {
  EXPECT_EQ(Sha1::hash("The quick brown fox jumps over the lazy dog"),
            "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

TEST(Sha1Test, OneMillionA) {
  Sha1 sha;
  const std::string chunk(1000, 'a');
  for (int i = 0; i < 1000; ++i) {
    sha.update(chunk);
  }
  EXPECT_EQ(sha.hexDigest(), "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

TEST(Sha1Test, ChunkedMatchesOneShot) {
  const std::string message =
      "MiniGit hashes this message one byte at a time and all at once.";
  Sha1 chunked;
  for (char c : message) {
    chunked.update(&c, 1);
  }
  EXPECT_EQ(chunked.hexDigest(), Sha1::hash(message));
}

TEST(Sha1Test, DigestIsRawTwentyBytes) {
  Sha1 sha;
  sha.update("hello");
  const auto digest = sha.digest();
  EXPECT_EQ(digest.size(), std::size_t{20});
  EXPECT_EQ(sha.hexDigest().size(), std::size_t{40});
}
