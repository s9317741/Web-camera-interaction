#include "Platform.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#else
#include <unistd.h>
#include <climits>
#endif

namespace app {

std::filesystem::path executableDir()
{
#if defined(_WIN32)
	std::wstring buffer(32768, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, &buffer[0], static_cast<DWORD>(buffer.size()));
	if (length == 0 || length >= buffer.size())
		return {};
	buffer.resize(length);
	return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
	std::uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::string buffer(size, '\0');
	if (_NSGetExecutablePath(&buffer[0], &size) != 0)
		return {};
	std::error_code ec;
	const std::filesystem::path resolved = std::filesystem::canonical(buffer.c_str(), ec);
	return ec ? std::filesystem::path(buffer.c_str()).parent_path() : resolved.parent_path();
#else
	char buffer[PATH_MAX];
	const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (length <= 0)
		return {};
	buffer[length] = '\0';
	return std::filesystem::path(buffer).parent_path();
#endif
}

std::filesystem::path pathFromUtf8(const std::string& utf8)
{
#if defined(_WIN32)
	if (utf8.empty())
		return {};
	const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	std::wstring wide(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &wide[0], length);
	return std::filesystem::path(wide);
#else
	return std::filesystem::path(utf8);
#endif
}

std::string pathToUtf8(const std::filesystem::path& path)
{
#if defined(_WIN32)
	const std::wstring wide = path.wstring();
	if (wide.empty())
		return {};
	const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
		nullptr, 0, nullptr, nullptr);
	std::string utf8(static_cast<std::size_t>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), &utf8[0], length,
		nullptr, nullptr);
	return utf8;
#else
	return path.string();
#endif
}

std::string timestamp()
{
	const std::time_t now = std::time(nullptr);
	std::tm local{};
#if defined(_WIN32)
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	char text[32];
	std::strftime(text, sizeof(text), "%Y%m%d-%H%M%S", &local);
	return text;
}

bool readFile(const std::filesystem::path& path, std::vector<unsigned char>& bytes)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

bool writeFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		return false;
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return static_cast<bool>(out);
}

bool openWithDefaultApp(const std::filesystem::path& path)
{
#if defined(_WIN32)
	const HINSTANCE result = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR>(result) > 32;
#else
	// Single quotes, with any single quote in the path closed, escaped, and reopened.
	std::string quoted = "'";
	for (const char c : path.string())
		quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
	quoted += "'";
#if defined(__APPLE__)
	const std::string command = "open " + quoted;
#else
	const std::string command = "xdg-open " + quoted + " >/dev/null 2>&1 &";
#endif
	return std::system(command.c_str()) == 0;
#endif
}

bool copyBgrToClipboard(const unsigned char* bgr, int width, int height, std::size_t stride)
{
#if defined(_WIN32)
	if (bgr == nullptr || width <= 0 || height <= 0)
		return false;
	// CF_DIB: header, then bottom-up rows padded to 4 bytes.
	const std::size_t rowBytes = (static_cast<std::size_t>(width) * 3 + 3) & ~static_cast<std::size_t>(3);
	const std::size_t size = sizeof(BITMAPINFOHEADER) + rowBytes * height;
	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size);
	if (memory == nullptr)
		return false;
	unsigned char* dib = static_cast<unsigned char*>(GlobalLock(memory));
	BITMAPINFOHEADER* header = reinterpret_cast<BITMAPINFOHEADER*>(dib);
	header->biSize = sizeof(BITMAPINFOHEADER);
	header->biWidth = width;
	header->biHeight = height;
	header->biPlanes = 1;
	header->biBitCount = 24;
	header->biCompression = BI_RGB;
	header->biSizeImage = static_cast<DWORD>(rowBytes * height);
	unsigned char* pixels = dib + sizeof(BITMAPINFOHEADER);
	for (int y = 0; y < height; ++y)
		std::memcpy(pixels + rowBytes * (height - 1 - y), bgr + stride * y, static_cast<std::size_t>(width) * 3);
	GlobalUnlock(memory);

	if (!OpenClipboard(nullptr))
	{
		GlobalFree(memory);
		return false;
	}
	EmptyClipboard();
	const bool ok = SetClipboardData(CF_DIB, memory) != nullptr;
	CloseClipboard();
	if (!ok)
		GlobalFree(memory);
	return ok;
#else
	(void)bgr;
	(void)width;
	(void)height;
	(void)stride;
	return false;
#endif
}

} // namespace app
