#include "ScreenGrab.h"

#include "Platform.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

namespace app {

namespace {

const int kMaxWidth = 1280;
// 30 a second, like most cameras, so their expressions come through as they
// happen. A full 1080p screen takes about 20 ms to copy; a call window less.
const auto kInterval = std::chrono::milliseconds(33);

#if defined(_WIN32)

BOOL CALLBACK collectWindow(HWND window, LPARAM param)
{
	std::vector<ScreenWindow>& windows = *reinterpret_cast<std::vector<ScreenWindow>*>(param);
	DWORD process = 0;
	GetWindowThreadProcessId(window, &process);
	if (process == GetCurrentProcessId() || !IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr ||
		(GetWindowLongW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0)
		return TRUE;
	// Windows of apps that are suspended or on another desktop are "cloaked": listed, never shown.
	DWORD cloaked = 0;
	if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
		return TRUE;
	const int length = GetWindowTextLengthW(window);
	if (length <= 0)
		return TRUE;
	std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');
	title.resize(static_cast<std::size_t>(GetWindowTextW(window, &title[0], length + 1)));
	if (title.empty())
		return TRUE;
	ScreenWindow entry;
	entry.handle = reinterpret_cast<std::uintptr_t>(window);
	entry.title = pathToUtf8(std::filesystem::path(title));
	windows.push_back(entry);
	return TRUE;
}

#endif

} // namespace

bool screenGrabSupported()
{
#if defined(_WIN32)
	return true;
#else
	return false;
#endif
}

std::vector<ScreenWindow> listScreenWindows()
{
	std::vector<ScreenWindow> windows;
#if defined(_WIN32)
	EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&windows));
#endif
	return windows;
}

bool grabScreenWindow(std::uintptr_t handle, cv::Mat& bgr, const cv::Rect2f& part)
{
#if defined(_WIN32)
	RECT rect{};
	if (handle == 0)
	{
		rect.right = GetSystemMetrics(SM_CXSCREEN);
		rect.bottom = GetSystemMetrics(SM_CYSCREEN);
	}
	else
	{
		const HWND window = reinterpret_cast<HWND>(handle);
		if (!IsWindow(window) || IsIconic(window))
			return false;
		// The visible frame, without the invisible resize border.
		if (FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect))))
			GetWindowRect(window, &rect);
	}
	// The chosen piece of it.
	const float fullWidth = static_cast<float>(rect.right - rect.left);
	const float fullHeight = static_cast<float>(rect.bottom - rect.top);
	const cv::Rect2f piece = part & cv::Rect2f(0, 0, 1, 1);
	if (piece.width <= 0.0f || piece.height <= 0.0f)
		return false;
	const LONG x = rect.left, y = rect.top;
	rect.left = x + static_cast<LONG>(piece.x * fullWidth);
	rect.top = y + static_cast<LONG>(piece.y * fullHeight);
	rect.right = x + static_cast<LONG>((piece.x + piece.width) * fullWidth);
	rect.bottom = y + static_cast<LONG>((piece.y + piece.height) * fullHeight);
	// Only what is on some screen can be copied.
	const int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	rect.left = std::max<LONG>(rect.left, left);
	rect.top = std::max<LONG>(rect.top, top);
	rect.right = std::min<LONG>(rect.right, left + GetSystemMetrics(SM_CXVIRTUALSCREEN));
	rect.bottom = std::min<LONG>(rect.bottom, top + GetSystemMetrics(SM_CYVIRTUALSCREEN));
	const int width = rect.right - rect.left, height = rect.bottom - rect.top;
	if (width < 16 || height < 16)
		return false;

	HDC screen = GetDC(nullptr);
	HDC memory = CreateCompatibleDC(screen);
	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height; // rows top first
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	bool ok = false;
	if (bitmap != nullptr)
	{
		HGDIOBJ old = SelectObject(memory, bitmap);
		ok = BitBlt(memory, 0, 0, width, height, screen, rect.left, rect.top, SRCCOPY) != 0;
		GdiFlush();
		if (ok)
			cv::cvtColor(cv::Mat(height, width, CV_8UC4, bits), bgr, cv::COLOR_BGRA2BGR);
		SelectObject(memory, old);
		DeleteObject(bitmap);
	}
	DeleteDC(memory);
	ReleaseDC(nullptr, screen);
	return ok;
#else
	(void)handle;
	(void)bgr;
	(void)part;
	return false;
#endif
}

ScreenGrabber::ScreenGrabber()
{
	// Started here, not in the initializer list: every member must exist first.
	thread_ = std::thread(&ScreenGrabber::run, this);
}

ScreenGrabber::~ScreenGrabber()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stop_ = true;
	}
	wake_.notify_all();
	thread_.join();
}

bool ScreenGrabber::loadFaceTracker(const std::filesystem::path& model, std::string& error)
{
	std::lock_guard<std::mutex> lock(mutex_);
	faceTrackerReady_ = faceTracker_.load(model, error);
	return faceTrackerReady_;
}

void ScreenGrabber::watch(std::uintptr_t window, const cv::Rect2f& part)
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		mode_ = Mode::Window;
		window_ = window;
		part_ = part;
		still_.release();
		frame_ = FaceFrame();
		lost_ = false;
		++request_;
	}
	wake_.notify_one();
}

void ScreenGrabber::show(const cv::Mat& bgr)
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		mode_ = Mode::Still;
		still_ = bgr.clone();
		frame_ = FaceFrame();
		lost_ = false;
		++request_;
	}
	wake_.notify_one();
}

void ScreenGrabber::stop()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		mode_ = Mode::Off;
		still_.release();
		frame_ = FaceFrame();
		lost_ = false;
		++request_;
	}
	wake_.notify_one();
}

FaceFrame ScreenGrabber::latest() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return frame_; // each picture gets a buffer of its own, so sharing is safe
}

void ScreenGrabber::run()
{
	using Clock = std::chrono::steady_clock;
	FaceSmoother smoother;
	std::uint64_t handled = 0;
	std::uint64_t sequence = 0;
	Clock::time_point due = Clock::now();
	for (;;)
	{
		Mode mode = Mode::Off;
		std::uintptr_t window = 0;
		cv::Rect2f part;
		cv::Mat still;
		std::uint64_t request = 0;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			// A still picture is looked at once; a window again and again, on a
			// schedule, so the time spent copying and looking comes off the wait.
			if (mode_ == Mode::Window)
			{
				due += kInterval;
				if (due < Clock::now())
					due = Clock::now(); // fell behind: carry on from now
				wake_.wait_until(lock, due, [&] { return stop_ || request_ != handled; });
			}
			else
				wake_.wait(lock, [&] { return stop_ || request_ != handled; });
			if (stop_)
				return;
			mode = mode_;
			window = window_;
			part = part_;
			still = still_;
			request = request_;
		}
		if (request != handled)
		{
			smoother.reset();
			handled = request;
		}

		cv::Mat picture;
		if (mode == Mode::Still)
			picture = still;
		else if (mode == Mode::Window)
		{
			cv::Mat grabbed;
			lost_ = !grabScreenWindow(window, grabbed, part);
			if (lost_)
				continue;
			if (grabbed.cols > kMaxWidth)
				cv::resize(grabbed, picture, cv::Size(), static_cast<double>(kMaxWidth) / grabbed.cols,
					static_cast<double>(kMaxWidth) / grabbed.cols, cv::INTER_AREA);
			else
				picture = grabbed;
		}
		if (picture.empty())
			continue;

		std::vector<Face> faces;
		if (faceTrackerReady_)
			faces = mode == Mode::Still ? faceTracker_.detect(picture) : smoother.update(faceTracker_.detect(picture));
		std::lock_guard<std::mutex> lock(mutex_);
		if (request_ != request) // switched while this one was being looked at
			continue;
		frame_.bgr = picture;
		frame_.faces = std::move(faces);
		frame_.sequence = ++sequence;
	}
}

} // namespace app
