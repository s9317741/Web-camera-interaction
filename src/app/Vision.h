#ifndef WEBCAM_APP_VISION_H
#define WEBCAM_APP_VISION_H

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace app {

// Finds the people in a picture with PP-HumanSeg (OpenCV Zoo, 192 x 192 input).
class PersonSegmenter
{
public:
	bool load(const std::filesystem::path& model, std::string& error);
	bool ready() const { return ready_; }

	// How likely each pixel is part of a person, 0..255, at the picture's size.
	bool segment(const cv::Mat& bgr, cv::Mat& alpha);

private:
	cv::dnn::Net net_;
	bool ready_ = false;
	cv::Mat probability_;
};

// A face found by FaceTracker, in picture coordinates. "Right" and "left"
// are as seen in the picture, so a mirrored picture needs no special case.
struct Face
{
	cv::Rect2f box;
	cv::Point2f eyeLeft;    // the eye nearer the picture's left edge
	cv::Point2f eyeRight;
	cv::Point2f nose;
	cv::Point2f mouth;      // between the mouth corners
	float score = 0.0f;

	cv::Point2f eyesCenter() const { return (eyeLeft + eyeRight) * 0.5f; }
	float eyeDistance() const;
	float tiltDegrees() const; // positive when the face leans clockwise
};

// Finds faces with YuNet (OpenCV Zoo) on a copy no wider than 320 pixels.
class FaceTracker
{
public:
	bool load(const std::filesystem::path& model, std::string& error);
	bool ready() const { return !detector_.empty(); }
	std::vector<Face> detect(const cv::Mat& bgr);

private:
	cv::Ptr<cv::FaceDetectorYN> detector_;
	cv::Size inputSize_;
	cv::Mat small_;
};

// Runs the segmenter and the face tracker on a thread of its own, so the
// picture never waits for them: each frame hands over a copy, and uses the
// newest results, which are one frame or so behind. The models run on the
// CPU; on the integrated GPUs tried they were slower through OpenCL.
class AiWorker
{
public:
	struct Result
	{
		std::uint64_t sequence = 0; // increases with every finished run
		cv::Mat alpha;              // person probability at the submitted size, or empty
		std::vector<Face> faces;
		cv::Mat picture;            // the picture the faces were found in, when faces were asked for
		double segmentMs = 0.0;
		double facesMs = 0.0;
	};

	AiWorker();
	~AiWorker();
	AiWorker(const AiWorker&) = delete;
	AiWorker& operator=(const AiWorker&) = delete;

	bool loadSegmenter(const std::filesystem::path& model, std::string& error);
	bool loadFaceTracker(const std::filesystem::path& model, std::string& error);
	bool segmenterReady() const { return segmenterReady_; }
	bool faceTrackerReady() const { return faceTrackerReady_; }

	// Replaces any picture still waiting; the worker always takes the newest.
	void submit(const cv::Mat& bgr, bool mask, bool faces);
	Result latest() const;

private:
	void run();

	PersonSegmenter segmenter_;
	FaceTracker faceTracker_;
	bool segmenterReady_ = false;
	bool faceTrackerReady_ = false;

	std::thread thread_;
	mutable std::mutex mutex_;
	std::condition_variable wake_;
	bool stop_ = false;
	cv::Mat pending_;
	bool pendingMask_ = false;
	bool pendingFaces_ = false;
	Result result_;
};

} // namespace app

#endif
