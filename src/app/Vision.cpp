#include "Vision.h"

#include "Platform.h"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cmath>

namespace app {

namespace {

const int kSegmentationSize = 192;
const int kFaceInputWidth = 320;
const float kFaceScoreThreshold = 0.8f;

} // namespace

bool PersonSegmenter::load(const std::filesystem::path& model, std::string& error)
{
	ready_ = false;
	std::vector<unsigned char> bytes;
	if (!readFile(model, bytes) || bytes.empty())
	{
		error = "Missing model " + pathToUtf8(model);
		return false;
	}
	try
	{
		// From memory: OpenCV takes narrow paths, which break on non-ASCII Windows paths.
		net_ = cv::dnn::readNetFromONNX(bytes);
		ready_ = !net_.empty();
	}
	catch (const cv::Exception& e)
	{
		error = e.what();
	}
	return ready_;
}

bool PersonSegmenter::segment(const cv::Mat& bgr, cv::Mat& alpha)
{
	if (!ready_ || bgr.empty())
		return false;
	// RGB, scaled to [-1, 1]: (pixel - 127.5) / 127.5.
	const cv::Mat blob = cv::dnn::blobFromImage(bgr, 1.0 / 127.5, cv::Size(kSegmentationSize, kSegmentationSize),
		cv::Scalar(127.5, 127.5, 127.5), true, false);
	net_.setInput(blob);
	const cv::Mat out = net_.forward();
	if (out.dims != 4 || out.size[1] != 2)
		return false;
	// Channel 1 is the probability of "person" (the two channels sum to 1).
	const cv::Mat person(out.size[2], out.size[3], CV_32F, const_cast<float*>(out.ptr<float>(0, 1)));
	cv::GaussianBlur(person, probability_, cv::Size(3, 3), 0);
	cv::Mat scaled;
	cv::resize(probability_, scaled, bgr.size(), 0, 0, cv::INTER_LINEAR);
	scaled.convertTo(alpha, CV_8U, 255.0);
	return true;
}

float Face::eyeDistance() const
{
	const cv::Point2f d = eyeRight - eyeLeft;
	return std::sqrt(d.x * d.x + d.y * d.y);
}

float Face::tiltDegrees() const
{
	const cv::Point2f d = eyeRight - eyeLeft;
	return static_cast<float>(std::atan2(d.y, d.x) * 180.0 / CV_PI);
}

bool FaceTracker::load(const std::filesystem::path& model, std::string& error)
{
	detector_.reset();
	std::vector<unsigned char> bytes;
	if (!readFile(model, bytes) || bytes.empty())
	{
		error = "Missing model " + pathToUtf8(model);
		return false;
	}
	try
	{
		inputSize_ = cv::Size(kFaceInputWidth, kFaceInputWidth * 3 / 4);
		detector_ = cv::FaceDetectorYN::create("onnx", bytes, std::vector<uchar>(), inputSize_,
			kFaceScoreThreshold, 0.3f, 50);
	}
	catch (const cv::Exception& e)
	{
		error = e.what();
		detector_.reset();
	}
	return !detector_.empty();
}

std::vector<Face> FaceTracker::detect(const cv::Mat& bgr)
{
	std::vector<Face> faces;
	if (detector_.empty() || bgr.empty())
		return faces;
	const double scale = std::min(1.0, static_cast<double>(kFaceInputWidth) / bgr.cols);
	cv::resize(bgr, small_, cv::Size(), scale, scale, cv::INTER_AREA);
	if (small_.size() != inputSize_)
	{
		inputSize_ = small_.size();
		detector_->setInputSize(inputSize_);
	}
	cv::Mat found;
	detector_->detect(small_, found);
	const float back = static_cast<float>(1.0 / scale);
	for (int i = 0; i < found.rows; ++i)
	{
		// x, y, w, h, then five landmarks (two eyes, nose, two mouth corners), then the score.
		const float* r = found.ptr<float>(i);
		Face face;
		face.box = cv::Rect2f(r[0] * back, r[1] * back, r[2] * back, r[3] * back);
		cv::Point2f a(r[4] * back, r[5] * back), b(r[6] * back, r[7] * back);
		if (a.x > b.x)
			std::swap(a, b);
		face.eyeLeft = a;
		face.eyeRight = b;
		face.nose = cv::Point2f(r[8] * back, r[9] * back);
		face.mouth = cv::Point2f((r[10] + r[12]) * 0.5f * back, (r[11] + r[13]) * 0.5f * back);
		face.score = r[14];
		faces.push_back(face);
	}
	return faces;
}

AiWorker::AiWorker()
{
	// Started here, not in the initializer list: every member, the mutex
	// included, must exist before the thread touches them.
	thread_ = std::thread(&AiWorker::run, this);
}

AiWorker::~AiWorker()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stop_ = true;
	}
	wake_.notify_all();
	thread_.join();
}

bool AiWorker::loadSegmenter(const std::filesystem::path& model, std::string& error)
{
	std::lock_guard<std::mutex> lock(mutex_);
	segmenterReady_ = segmenter_.load(model, error);
	return segmenterReady_;
}

bool AiWorker::loadFaceTracker(const std::filesystem::path& model, std::string& error)
{
	std::lock_guard<std::mutex> lock(mutex_);
	faceTrackerReady_ = faceTracker_.load(model, error);
	return faceTrackerReady_;
}

void AiWorker::submit(const cv::Mat& bgr, bool mask, bool faces)
{
	if (bgr.empty() || (!mask && !faces))
		return;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bgr.copyTo(pending_);
		pendingMask_ = mask && segmenterReady_;
		pendingFaces_ = faces && faceTrackerReady_;
	}
	wake_.notify_one();
}

AiWorker::Result AiWorker::latest() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return result_; // the worker never writes into these Mats again, so sharing is safe
}

void AiWorker::run()
{
	using Clock = std::chrono::steady_clock;
	cv::Mat picture;
	for (;;)
	{
		bool mask = false, faces = false;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			wake_.wait(lock, [this] { return stop_ || !pending_.empty(); });
			if (stop_)
				return;
			cv::swap(picture, pending_);
			pending_.release();
			mask = pendingMask_;
			faces = pendingFaces_;
		}

		Result next;
		if (mask)
		{
			const Clock::time_point start = Clock::now();
			if (!segmenter_.segment(picture, next.alpha))
				next.alpha.release();
			next.segmentMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
		}
		if (faces)
		{
			const Clock::time_point start = Clock::now();
			next.faces = faceTracker_.detect(picture);
			next.picture = picture; // the next swap moves on to a new buffer, so this one stays as it is
			next.facesMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
		}

		std::lock_guard<std::mutex> lock(mutex_);
		next.sequence = result_.sequence + 1;
		result_ = std::move(next);
	}
}

} // namespace app
