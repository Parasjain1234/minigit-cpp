#include "hash/sha1.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace minigit {
namespace {

inline std::uint32_t rotateLeft(std::uint32_t value, int bits) {
  return (value << bits) | (value >> (32 - bits));
}

inline std::uint32_t readBigEndian(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) |
         (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) |
         static_cast<std::uint32_t>(p[3]);
}

}  // namespace

Sha1::Sha1()
    : state_{0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u,
             0xC3D2E1F0u} {}

void Sha1::update(const void* data, std::size_t length) {
  if (finalized_) {
    throw std::logic_error("Sha1::update() called after finalisation");
  }
  if (length == 0) {
    return;
  }

  const auto* bytes = static_cast<const std::uint8_t*>(data);
  bitCount_ += static_cast<std::uint64_t>(length) * 8u;

  while (length > 0) {
    const std::size_t take =
        std::min(length, kBlockSize - bufferLength_);
    std::memcpy(buffer_.data() + bufferLength_, bytes, take);
    bufferLength_ += take;
    bytes += take;
    length -= take;

    if (bufferLength_ == kBlockSize) {
      processBlock(buffer_.data());
      bufferLength_ = 0;
    }
  }
}

void Sha1::processBlock(const std::uint8_t* block) {
  std::array<std::uint32_t, 80> w{};
  for (std::size_t i = 0; i < 16; ++i) {
    w[i] = readBigEndian(block + i * 4);
  }
  for (std::size_t i = 16; i < 80; ++i) {
    w[i] = rotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }

  std::uint32_t a = state_[0];
  std::uint32_t b = state_[1];
  std::uint32_t c = state_[2];
  std::uint32_t d = state_[3];
  std::uint32_t e = state_[4];

  for (std::size_t i = 0; i < 80; ++i) {
    std::uint32_t f = 0;
    std::uint32_t k = 0;
    if (i < 20) {
      f = (b & c) | (~b & d);
      k = 0x5A827999u;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1u;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCu;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6u;
    }

    const std::uint32_t temp =
        rotateLeft(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rotateLeft(b, 30);
    b = a;
    a = temp;
  }

  state_[0] += a;
  state_[1] += b;
  state_[2] += c;
  state_[3] += d;
  state_[4] += e;
}

void Sha1::finalize() {
  if (finalized_) {
    return;
  }

  const std::uint64_t messageBits = bitCount_;

  // Append the mandatory 0x80 byte, then zeros, then the 64 bit big-endian
  // message length.  Padding must leave 8 bytes for the length field.
  std::uint8_t padding = 0x80;
  buffer_[bufferLength_++] = padding;
  if (bufferLength_ > kBlockSize - 8) {
    std::memset(buffer_.data() + bufferLength_, 0, kBlockSize - bufferLength_);
    processBlock(buffer_.data());
    bufferLength_ = 0;
  }
  std::memset(buffer_.data() + bufferLength_, 0, kBlockSize - 8 - bufferLength_);
  for (std::size_t i = 0; i < 8; ++i) {
    buffer_[kBlockSize - 1 - i] =
        static_cast<std::uint8_t>(messageBits >> (i * 8));
  }
  processBlock(buffer_.data());
  bufferLength_ = 0;
  finalized_ = true;
}

std::array<std::uint8_t, Sha1::kDigestSize> Sha1::digest() {
  finalize();
  std::array<std::uint8_t, kDigestSize> out{};
  for (std::size_t i = 0; i < state_.size(); ++i) {
    out[i * 4 + 0] = static_cast<std::uint8_t>(state_[i] >> 24);
    out[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
    out[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
    out[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
  }
  return out;
}

std::string Sha1::hexDigest() {
  static constexpr char kHex[] = "0123456789abcdef";
  const auto raw = digest();
  std::string out;
  out.reserve(raw.size() * 2);
  for (std::uint8_t byte : raw) {
    out.push_back(kHex[byte >> 4]);
    out.push_back(kHex[byte & 0x0F]);
  }
  return out;
}

std::string Sha1::hash(const std::string& data) {
  Sha1 sha;
  sha.update(data);
  return sha.hexDigest();
}

}  // namespace minigit
