#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace minigit_test {

/// Creates a unique temporary directory and removes it on destruction.
class TempDir {
 public:
  explicit TempDir(const std::string& tag = "minigit") {
    static std::atomic<unsigned> counter{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (tag + "_" + std::to_string(now) + "_" +
             std::to_string(counter.fetch_add(1)));
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
    std::filesystem::create_directories(path_, ec);
  }

  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

inline void writeTextFile(const std::filesystem::path& path,
                          const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << contents;
}

inline std::string readTextFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

}  // namespace minigit_test
