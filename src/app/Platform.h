#ifndef WEBCAM_APP_PLATFORM_H
#define WEBCAM_APP_PLATFORM_H

#include <filesystem>
#include <string>
#include <vector>

namespace app {

// Folder holding the running executable (empty if it cannot be found).
std::filesystem::path executableDir();

// UI text is UTF-8; these convert to and from native paths on every platform.
std::filesystem::path pathFromUtf8(const std::string& utf8);
std::string pathToUtf8(const std::filesystem::path& path);

// Local time as "20261027-153012", for file names.
std::string timestamp();

// Whole-file I/O that works with non-ASCII paths on Windows, where OpenCV's
// imread/imwrite take narrow strings.
bool readFile(const std::filesystem::path& path, std::vector<unsigned char>& bytes);
bool writeFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes);

// Opens a file or folder with the system's default program.
bool openWithDefaultApp(const std::filesystem::path& path);

// Puts 8-bit BGR pixels (rows top to bottom) on the clipboard as a bitmap.
// Windows only; returns false elsewhere.
bool copyBgrToClipboard(const unsigned char* bgr, int width, int height, std::size_t stride);

} // namespace app

#endif
