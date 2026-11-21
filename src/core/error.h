#pragma once

#include <stdexcept>
#include <string>

namespace minigit {

/// Every expected failure (bad usage, corrupt repository, missing object)
/// is reported by throwing MinigitError.  main() catches it and prints a
/// single human readable line, so the command never dies with a raw
/// std::terminate or an unhelpful "terminate called".
class MinigitError : public std::runtime_error {
 public:
  explicit MinigitError(const std::string& message)
      : std::runtime_error(message) {}
};

}  // namespace minigit
