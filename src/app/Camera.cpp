#include "Camera.h"

#include <chrono>
#include <cmath>
#include <limits>

namespace app {

const std::vector<CameraBackend>& cameraBackends()
{
	static const std::vector<CameraBackend> backends = {
		{ "Auto", cv::CAP_ANY },
#if defined(_WIN32)
		{ "Media Foundation", cv::CAP_MSMF },
		{ "DirectShow", cv::CAP_DSHOW },
#elif defined(__APPLE__)
		{ "AVFoundation", cv::CAP_AVFOUNDATION },
#else
		{ "V4L2", cv::CAP_V4L2 },
#endif
	};
	return backends;
}

const std::vector<std::pair<int, const char*>>& Camera::adjustableProperties()
{
	static const std::vector<std::pair<int, const char*>> props = {
		{ cv::CAP_PROP_AUTO_EXPOSURE, "Auto exposure" },
		{ cv::CAP_PROP_EXPOSURE, "Exposure" },
		{ cv::CAP_PROP_GAIN, "Gain" },
		{ cv::CAP_PROP_AUTO_WB, "Auto white balance" },
		{ cv::CAP_PROP_WB_TEMPERATURE, "White balance" },
		{ cv::CAP_PROP_BRIGHTNESS, "Brightness" },
		{ cv::CAP_PROP_CONTRAST, "Contrast" },
		{ cv::CAP_PROP_SATURATION, "Saturation" },
		{ cv::CAP_PROP_AUTOFOCUS, "Autofocus" },
		{ cv::CAP_PROP_FOCUS, "Focus" },
	};
	return props;
}

void Camera::open(const Request& request)
{
	close();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		error_.clear();
		info_ = Info();
		frame_.release();
		properties_.clear();
		pendingSets_.clear();
		settingsRequested_ = false;
	}
	stop_ = false;
	measuredFps_ = 0.0;
	state_ = State::Opening;
	thread_ = std::thread(&Camera::run, this, request);
}

void Camera::close()
{
	stop_ = true;
	if (thread_.joinable())
		thread_.join();
	if (state_ != State::Failed)
		state_ = State::Closed;
}

std::string Camera::error() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return error_;
}

Camera::Info Camera::info() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return info_;
}

bool Camera::latest(cv::Mat& frame, std::uint64_t& seen) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (sequence_ == seen || frame_.empty())
		return false;
	frame_.copyTo(frame);
	seen = sequence_;
	return true;
}

double Camera::property(int id) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	const std::map<int, double>::const_iterator it = properties_.find(id);
	return it == properties_.end() ? std::numeric_limits<double>::quiet_NaN() : it->second;
}

void Camera::setProperty(int id, double value)
{
	std::lock_guard<std::mutex> lock(mutex_);
	pendingSets_.emplace_back(id, value);
}

void Camera::openSettingsDialog()
{
	std::lock_guard<std::mutex> lock(mutex_);
	settingsRequested_ = true;
}

void Camera::refreshProperties(cv::VideoCapture& capture)
{
	std::map<int, double> values;
	for (const auto& prop : adjustableProperties())
		values[prop.first] = capture.get(prop.first);
	std::lock_guard<std::mutex> lock(mutex_);
	properties_.swap(values);
}

void Camera::run(Request request)
{
	cv::VideoCapture capture;
	try
	{
		capture.open(request.index, request.api);
		if (capture.isOpened())
		{
			// Format first: some drivers only offer large sizes in MJPG.
			if (request.preferMjpg)
				capture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
			capture.set(cv::CAP_PROP_FRAME_WIDTH, request.width);
			capture.set(cv::CAP_PROP_FRAME_HEIGHT, request.height);
			// Many cameras stay at 30 fps unless asked; the driver picks the
			// nearest rate it supports at this size and format.
			if (request.fps > 0)
				capture.set(cv::CAP_PROP_FPS, request.fps);
		}
	}
	catch (const cv::Exception& e)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		error_ = e.what();
	}
	if (!capture.isOpened())
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (error_.empty())
			error_ = "Cannot open camera " + std::to_string(request.index) +
				" with this backend. Is it connected, or in use by another program?";
		state_ = State::Failed;
		return;
	}

	{
		Info info;
		info.backend = capture.getBackendName();
		info.width = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
		info.height = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
		info.fps = capture.get(cv::CAP_PROP_FPS);
		const int code = static_cast<int>(capture.get(cv::CAP_PROP_FOURCC));
		for (int i = 0; i < 4; ++i)
		{
			const char c = static_cast<char>((code >> (8 * i)) & 0xFF);
			info.fourcc += (c >= 32 && c < 127) ? c : '?';
		}
		std::lock_guard<std::mutex> lock(mutex_);
		info_ = info;
	}
	refreshProperties(capture);
	state_ = State::Running;

	using Clock = std::chrono::steady_clock;
	Clock::time_point windowStart = Clock::now();
	int framesInWindow = 0;
	int failures = 0;
	cv::Mat frame;
	while (!stop_)
	{
		std::deque<std::pair<int, double>> sets;
		bool settings = false;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			sets.swap(pendingSets_);
			settings = settingsRequested_;
			settingsRequested_ = false;
		}
		if (!sets.empty() || settings)
		{
			for (const auto& set : sets)
				capture.set(set.first, set.second);
			if (settings)
				capture.set(cv::CAP_PROP_SETTINGS, 1);
			refreshProperties(capture);
		}

		if (!capture.read(frame) || frame.empty())
		{
			if (++failures > 100)
			{
				std::lock_guard<std::mutex> lock(mutex_);
				error_ = "The camera stopped sending frames.";
				state_ = State::Failed;
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}
		failures = 0;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			cv::swap(frame_, frame);
			++sequence_;
		}

		++framesInWindow;
		const double elapsed = std::chrono::duration<double>(Clock::now() - windowStart).count();
		if (elapsed >= 1.0)
		{
			measuredFps_ = framesInWindow / elapsed;
			framesInWindow = 0;
			windowStart = Clock::now();
		}
	}
}

} // namespace app
