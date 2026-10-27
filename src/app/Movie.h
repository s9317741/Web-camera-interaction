#ifndef WEBCAM_APP_MOVIE_H
#define WEBCAM_APP_MOVIE_H

#include "Vision.h"

#include <opencv2/core.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace app {

// Funhouse-mirror changes to a pasted face.
enum class Warp { None, BigEyes, BigMouth, Wide, Long, Count };

struct PasteStyle
{
	float jaw = 0.0f;       // 0..1: how far the lower face drops open, as if singing
	Warp warp = Warp::None;
	bool recolor = true;    // match the skin it covers (off when a face is pasted onto itself)
};

// Puts the face in `from` over the face `onto` in `picture`: turned, scaled
// and moved so the eyes, nose and mouth line up, recolored to match the skin
// it covers, and faded out towards the edge of an oval. Does nothing for
// faces too small to place. To pull faces at a face without swapping it,
// paste a copy of the picture onto itself with `recolor` off.
void pasteFace(const cv::Mat& from, const Face& fromFace, cv::Mat& picture, const Face& onto,
	const PasteStyle& style = PasteStyle());

// A picture and the faces in it, from a video or a window on screen.
struct FaceFrame
{
	std::uint64_t sequence = 0; // increases with every new picture
	cv::Mat bgr;
	std::vector<Face> faces;
};

// Steadies faces found in a stream of pictures: small moves (mostly the
// detector's own noise) are damped, real movement comes through at once,
// and a face the detector misses is held for a few pictures.
class FaceSmoother
{
public:
	std::vector<Face> update(const std::vector<Face>& found);
	void reset() { tracks_.clear(); }

private:
	struct Track
	{
		Face face;
		int missed = 0;
	};
	std::vector<Track> tracks_;
};

// Which face goes where: the biggest of `people` onto the biggest of
// `actors`, the second onto the second, and so on. Pairs of indices.
std::vector<std::pair<int, int>> pairFaces(const std::vector<Face>& people, const std::vector<Face>& actors);

// Plays a video file on a thread of its own at the video's own speed, over
// and over, and finds the faces in every frame as it goes. Faces are smoothed
// from frame to frame and held for a few frames when the detector misses
// them, so a pasted face neither shakes nor flickers. No sound: OpenCV reads
// only the pictures.
class MoviePlayer
{
public:
	using Frame = FaceFrame; // no wider than kMaxWidth

	static const int kMaxWidth = 1280;

	MoviePlayer() = default;
	~MoviePlayer() { close(); }
	MoviePlayer(const MoviePlayer&) = delete;
	MoviePlayer& operator=(const MoviePlayer&) = delete;

	// Before open(): a player without a face model only plays.
	bool loadFaceTracker(const std::filesystem::path& model, std::string& error);
	bool faceTrackerReady() const { return faceTracker_.ready(); }

	void open(const std::filesystem::path& file); // returns at once; frames follow
	void close();
	bool playing() const { return thread_.joinable() && !failed_; }
	bool failed() const { return failed_; }

	void setPaused(bool paused) { paused_ = paused; }
	bool paused() const { return paused_; }
	void restart() { restart_ = true; }

	Frame latest() const;
	double seconds() const { return seconds_; }   // where playback is
	double duration() const { return duration_; } // 0 when the file does not say

	// The first frame of a video, for a thumbnail; empty when it cannot be read.
	static cv::Mat firstFrame(const std::filesystem::path& file);

private:
	void run(std::filesystem::path file);

	FaceTracker faceTracker_; // used by the playing thread only, once loaded
	std::thread thread_;
	std::atomic<bool> stop_{false};
	std::atomic<bool> failed_{false};
	std::atomic<bool> paused_{false};
	std::atomic<bool> restart_{false};
	std::atomic<double> seconds_{0.0};
	std::atomic<double> duration_{0.0};

	mutable std::mutex mutex_; // guards frame_
	Frame frame_;
};

} // namespace app

#endif
