#include "compress/huffman.h"

#include <gtest/gtest.h>

#include <string>

#include "core/error.h"

using minigit::Huffman;
using minigit::MinigitError;

TEST(HuffmanTest, EmptyInputRoundTrips) {
  const std::string compressed = Huffman::compress("");
  EXPECT_TRUE(Huffman::isCompressed(compressed));
  EXPECT_EQ(Huffman::decompress(compressed), "");
}

TEST(HuffmanTest, SingleRepeatedSymbolRoundTrips) {
  const std::string input(1000, 'a');
  const std::string compressed = Huffman::compress(input);
  EXPECT_EQ(Huffman::decompress(compressed), input);
  EXPECT_LT(compressed.size(), input.size());
}

TEST(HuffmanTest, RoundTripsPlainText) {
  const std::string input =
      "the quick brown fox jumps over the lazy dog\n"
      "pack my box with five dozen liquor jugs\n";
  EXPECT_EQ(Huffman::decompress(Huffman::compress(input)), input);
}

TEST(HuffmanTest, RoundTripsEveryByteValue) {
  std::string input;
  for (int repeat = 0; repeat < 4; ++repeat) {
    for (int byte = 0; byte < 256; ++byte) {
      input.push_back(static_cast<char>(byte));
    }
  }
  EXPECT_EQ(Huffman::decompress(Huffman::compress(input)), input);
}

TEST(HuffmanTest, RoundTripsObjectPayloads) {
  const std::string payload = std::string("blob 5000\0", 10) +
                              std::string(5000, 'x');
  EXPECT_EQ(Huffman::decompress(Huffman::compress(payload)), payload);
}

TEST(HuffmanTest, SkewedDistributionShrinks) {
  std::string input;
  for (int i = 0; i < 5000; ++i) {
    input.push_back('e');
    if (i % 100 == 0) {
      input.push_back('z');
    }
  }

  const std::string compressed = Huffman::compress(input);
  EXPECT_LT(compressed.size(), input.size() / 2);
  EXPECT_EQ(Huffman::decompress(compressed), input);
}

TEST(HuffmanTest, RejectsForeignData) {
  EXPECT_FALSE(Huffman::isCompressed("blob 5\0hello"));
  EXPECT_THROW(Huffman::decompress("not compressed"), MinigitError);
}

TEST(HuffmanTest, RejectsTruncatedStreams) {
  const std::string compressed = Huffman::compress("hello hello hello");
  for (std::size_t cut = 4; cut + 1 < compressed.size(); ++cut) {
    EXPECT_THROW(Huffman::decompress(compressed.substr(0, cut)), MinigitError)
        << "cut at " << cut;
  }
}
