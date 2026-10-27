#ifndef WEBCAM_APP_CAMERA_H
#define WEBCAM_APP_CAMERA_H

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace app {

struct CameraBackend
{
	const char* name;
	int api; // cv::VideoCaptureAPIs
};

// The capture APIs worth offering on this platform; the first is "Auto".
const std::vector<CameraBackend>& cameraBackends();

// A camera owned by a background thread. Opening, reading, and changing
// properties all happen on that thread, so the UI never waits on the driver.
class Camera
{
public:
	enum class State { Closed, Opening, Running, Failed };

	struct Request
	{
		int index = 0;
		int width = 640;
		int height = 480;
		int api = cv::CAP_ANY;
		bool preferMjpg = true; // compressed formats reach 30 fps at 1080p over USB 2
		int fps = 0;            // frame rate to ask for; 0 leaves the driver's default
	};

	struct Info
	{
		std::string backend;
		int width = 0;
		int height = 0;
		double fps = 0.0;    // as reported by the driver
		std::string fourcc;  // pixel format the camera delivers
	};

	// Properties shown in the UI; their meaning and range depend on the driver.
	static const std::vector<std::pair<int, const char*>>& adjustableProperties();

	Camera() = default;
	~Camera() { close(); }
	Camera(const Camera&) = delete;
	Camera& operator=(const Camera&) = delete;

	void open(const Request& request); // returns at once; watch state()
	void close();

	State state() const { return state_.load(); }
	std::string error() const;
	Info info() const;
	double measuredFps() const { return measuredFps_.load(); }

	// Copies the newest frame (8-bit BGR) if it is newer than `seen`.
	bool latest(cv::Mat& frame, std::uint64_t& seen) const;

	// Last value read back from the driver; NaN when unknown.
	double property(int id) const;
	void setProperty(int id, double value);
	// Driver settings window (DirectShow on Windows only). Modal while open.
	void openSettingsDialog();

private:
	void run(Request request);
	void refreshProperties(cv::VideoCapture& capture);

	std::thread thread_;
	std::atomic<bool> stop_{false};
	std::atomic<State> state_{State::Closed};
	std::atomic<double> measuredFps_{0.0};

	mutable std::mutex mutex_; // guards everything below
	std::string error_;
	Info info_;
	cv::Mat frame_;
	std::uint64_t sequence_ = 0;
	std::map<int, double> properties_;
	std::deque<std::pair<int, double>> pendingSets_;
	bool settingsRequested_ = false;
};

} // namespace app

#endif
