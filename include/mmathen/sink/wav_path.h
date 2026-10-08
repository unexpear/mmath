#pragma once

// Where a test or an example writes a WAV.
//
// The required file goes to MMATH_WAV_DIR when that variable is set and
// non-empty, otherwise to std::filesystem::temp_directory_path(). A
// listening copy is placed in /opt/cursor/artifacts only when that
// directory already exists. A missing artifacts directory is not an
// error: a Windows checkout and a local build do not have it.

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace mmathen {

inline constexpr const char* kArtifactWavDirectory = "/opt/cursor/artifacts";

[[nodiscard]] inline std::string wavDirectoryOverride() {
#if defined(_MSC_VER)
  char* buffer = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&buffer, &length, "MMATH_WAV_DIR") != 0 || buffer == nullptr)
    return {};
  std::string value(buffer);
  std::free(buffer);
  return value;
#else
  const char* value = std::getenv("MMATH_WAV_DIR");
  if (value == nullptr) return {};
  return std::string(value);
#endif
}

[[nodiscard]] inline std::filesystem::path wavOutputDirectory() {
  const std::string overrideDirectory = wavDirectoryOverride();
  if (!overrideDirectory.empty()) return std::filesystem::path(overrideDirectory);
  return std::filesystem::temp_directory_path();
}

[[nodiscard]] inline std::filesystem::path wavOutputPath(const std::string& filename) {
  return wavOutputDirectory() / filename;
}

// Empty when /opt/cursor/artifacts is not a directory.
[[nodiscard]] inline std::filesystem::path artifactWavPath(const std::string& filename) {
  const std::filesystem::path directory{kArtifactWavDirectory};
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error) || error) return {};
  return directory / filename;
}

// An empty destination means there is nowhere to copy. That is success.
[[nodiscard]] inline bool copyWavFile(const std::filesystem::path& source,
                                      const std::filesystem::path& destination) {
  if (destination.empty()) return true;
  std::error_code error;
  if (std::filesystem::exists(destination, error) && !error) {
    const bool same = std::filesystem::equivalent(source, destination, error);
    if (!error && same) return true;
  }
  error.clear();
  std::filesystem::copy_file(source, destination,
                             std::filesystem::copy_options::overwrite_existing, error);
  return !error;
}

[[nodiscard]] inline bool copyWavToArtifacts(const std::filesystem::path& source,
                                            const std::string& filename) {
  return copyWavFile(source, artifactWavPath(filename));
}

}  // namespace mmathen
