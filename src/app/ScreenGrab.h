#ifndef WEBCAM_APP_SCREENGRAB_H
#define WEBCAM_APP_SCREENGRAB_H

#include "Movie.h"
#include "Vision.h"

#include <opencv2/core.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace app {

// A window of another program, as listed for the user to pick.
struct ScreenWindow
{
	std::uintptr_t handle = 0; // 0 stands for the whole main screen
	std::string title;         // UTF-8
};

// Whether this platform can copy other windows off the screen. Windows only
// for now; elsewhere a screenshot dropped on the booth stands in.
bool screenGrabSupported();

// Visible windows of other programs, top first. Empty where unsupported.
std::vector<ScreenWindow> listScreenWindows();

// Copies what the screen shows over a window (or the whole main screen):
// covered parts come out as whatever covers them, and a minimized window
// cannot be copied. `part` is the piece of it to copy, as fractions of its
// size, so it stays put when the window moves or is resized; the smaller,
// the quicker. False when the window is gone or minimized.
bool grabScreenWindow(std::uintptr_t handle, cv::Mat& bgr, const cv::Rect2f& part = cv::Rect2f(0, 0, 1, 1));

// Watches a window on screen thirty times a second, live, or holds a still
// picture, on a thread of its own, and finds the faces in it.
class ScreenGrabber
{
public:
	ScreenGrabber();
	~ScreenGrabber();
	ScreenGrabber(const ScreenGrabber&) = delete;
	ScreenGrabber& operator=(const ScreenGrabber&) = delete;

	bool loadFaceTracker(const std::filesystem::path& model, std::string& error);
	bool faceTrackerReady() const { return faceTrackerReady_; }

	void watch(std::uintptr_t window, const cv::Rect2f& part = cv::Rect2f(0, 0, 1, 1)); // 0: the whole main screen
	void show(const cv::Mat& bgr);     // a still picture instead
	void stop();

	bool watching() const { return mode_ == Mode::Window; }
	bool showing() const { return mode_ == Mode::Still; }
	bool lost() const { return lost_; } // the watched window cannot be copied right now

	FaceFrame latest() const;

private:
	enum class Mode { Off, Window, Still };

	void run();

	FaceTracker faceTracker_;
	std::atomic<bool> faceTrackerReady_{false};

	std::thread thread_;
	mutable std::mutex mutex_; // guards everything below
	std::condition_variable wake_;
	bool stop_ = false;
	std::atomic<Mode> mode_{Mode::Off};
	std::atomic<bool> lost_{false};
	std::uintptr_t window_ = 0;
	cv::Rect2f part_;
	cv::Mat still_;
	std::uint64_t request_ = 0; // changes with every watch / show / stop
	FaceFrame frame_;
};

} // namespace app

#endif
