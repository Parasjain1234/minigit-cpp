#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace minigit {

/// SHA-1 (FIPS 180-1) implemented from scratch, no OpenSSL.
///
/// The digest is computed incrementally so that large files can be hashed
/// in fixed size chunks without loading them into memory twice.
class Sha1 {
 public:
  static constexpr std::size_t kDigestSize = 20;
  static constexpr std::size_t kBlockSize = 64;

  Sha1();

  /// Feeds bytes into the digest state.  Throws std::logic_error if called
  /// after digest()/hexDigest() has finalised the value.
  void update(const void* data, std::size_t length);
  void update(const std::string& data) {
    update(data.data(), data.size());
  }

  /// Pads the message and returns the 20 raw digest bytes.
  std::array<std::uint8_t, kDigestSize> digest();

  /// Lowercase hexadecimal form of digest(), e.g. 40 characters.
  std::string hexDigest();

  /// One-shot convenience helper over a byte string.
  static std::string hash(const std::string& data);

 private:
  void processBlock(const std::uint8_t* block);
  void finalize();

  std::array<std::uint32_t, 5> state_;
  std::uint64_t bitCount_ = 0;
  std::array<std::uint8_t, kBlockSize> buffer_{};
  std::size_t bufferLength_ = 0;
  bool finalized_ = false;
};

}  // namespace minigit
