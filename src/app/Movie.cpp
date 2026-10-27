#include "Movie.h"

#include "Platform.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

namespace app {

namespace {

using Clock = std::chrono::steady_clock;

const float kMinEyeDistance = 6.0f; // pixels; smaller faces are not worth a paste
const int kHoldFrames = 6;          // frames a face is kept after the detector loses it

float length(cv::Point2f v)
{
	return std::sqrt(v.x * v.x + v.y * v.y);
}

// The turn, scale and shift that best carries `from` onto `to` (least squares).
cv::Matx23d similarity(const cv::Point2f* from, const cv::Point2f* to, int count)
{
	cv::Point2d meanFrom, meanTo;
	for (int i = 0; i < count; ++i)
	{
		meanFrom += cv::Point2d(from[i]);
		meanTo += cv::Point2d(to[i]);
	}
	meanFrom *= 1.0 / count;
	meanTo *= 1.0 / count;
	double a = 0.0, b = 0.0, norm = 0.0;
	for (int i = 0; i < count; ++i)
	{
		const cv::Point2d p = cv::Point2d(from[i]) - meanFrom;
		const cv::Point2d q = cv::Point2d(to[i]) - meanTo;
		a += p.x * q.x + p.y * q.y;
		b += p.x * q.y - p.y * q.x;
		norm += p.x * p.x + p.y * p.y;
	}
	if (norm <= 0.0)
		return cv::Matx23d(1, 0, meanTo.x - meanFrom.x, 0, 1, meanTo.y - meanFrom.y);
	a /= norm;
	b /= norm;
	return cv::Matx23d(a, -b, meanTo.x - (a * meanFrom.x - b * meanFrom.y),
		b, a, meanTo.y - (b * meanFrom.x + a * meanFrom.y));
}

Face mix(const Face& a, const Face& b, float t)
{
	const auto lerp = [t](cv::Point2f p, cv::Point2f q) { return p + (q - p) * t; };
	Face face = b;
	const cv::Point2f topLeft = lerp(a.box.tl(), b.box.tl());
	const cv::Point2f size = lerp(cv::Point2f(a.box.width, a.box.height), cv::Point2f(b.box.width, b.box.height));
	face.box = cv::Rect2f(topLeft.x, topLeft.y, size.x, size.y);
	face.eyeLeft = lerp(a.eyeLeft, b.eyeLeft);
	face.eyeRight = lerp(a.eyeRight, b.eyeRight);
	face.nose = lerp(a.nose, b.nose);
	face.mouth = lerp(a.mouth, b.mouth);
	return face;
}

} // namespace

std::vector<Face> FaceSmoother::update(const std::vector<Face>& found)
{
	std::vector<Track>& tracks = tracks_;
	std::vector<bool> used(tracks.size(), false);
	std::vector<Track> next;
	for (const Face& face : found)
	{
		int best = -1;
		float bestDistance = 0.0f;
		for (std::size_t i = 0; i < tracks.size(); ++i)
		{
			if (used[i])
				continue;
			const float distance = length(tracks[i].face.eyesCenter() - face.eyesCenter());
			const float limit = 0.6f * std::max(tracks[i].face.eyeDistance(), face.eyeDistance());
			if (distance < limit && (best < 0 || distance < bestDistance))
			{
				best = static_cast<int>(i);
				bestDistance = distance;
			}
		}
		Track track;
		track.face = face;
		if (best >= 0)
		{
			used[best] = true;
			const float move = bestDistance / std::max(1.0f, face.eyeDistance());
			track.face = mix(tracks[best].face, face, std::clamp(move / 0.15f, 0.4f, 1.0f));
		}
		next.push_back(track);
	}
	for (std::size_t i = 0; i < tracks.size(); ++i)
		if (!used[i] && tracks[i].missed < kHoldFrames)
		{
			next.push_back(tracks[i]);
			++next.back().missed;
		}
	tracks.swap(next);
	std::vector<Face> faces;
	for (const Track& track : tracks)
		faces.push_back(track.face);
	return faces;
}

namespace {

bool openVideo(cv::VideoCapture& capture, const std::filesystem::path& file)
{
	// FFmpeg takes UTF-8 file names on every platform, Chinese ones included.
	const std::string name = pathToUtf8(file);
	try
	{
		if (capture.open(name, cv::CAP_FFMPEG))
			return true;
		return capture.open(name, cv::CAP_ANY);
	}
	catch (const cv::Exception&)
	{
		return false;
	}
}

} // namespace

void pasteFace(const cv::Mat& from, const Face& fromFace, cv::Mat& picture, const Face& onto, const PasteStyle& style)
{
	const float d = onto.eyeDistance();
	if (from.empty() || picture.empty() || from.type() != CV_8UC3 || picture.type() != CV_8UC3 ||
		d < kMinEyeDistance || fromFace.eyeDistance() < kMinEyeDistance)
		return;

	// The oval of the face being covered: eyebrows to chin, cheek to cheek,
	// along the line of the eyes.
	const cv::Point2f eyes = onto.eyesCenter();
	const cv::Point2f across = (onto.eyeRight - onto.eyeLeft) * (1.0f / d);
	const cv::Point2f down(-across.y, across.x);
	const float eyesToMouth = std::max(0.6f * d, (onto.mouth - eyes).dot(down));
	const cv::Point2f top = eyes - down * (0.55f * d);
	const cv::Point2f bottom = eyes + down * (eyesToMouth + 0.5f * d);
	const cv::Point2f center = (top + bottom) * 0.5f;
	const cv::Size2f axes(0.95f * d, length(bottom - top) * 0.5f);
	const float angle = static_cast<float>(std::atan2(across.y, across.x) * 180.0 / CV_PI);

	// Room for what the style adds: a dropped jaw, a stretched face.
	const float jaw = std::clamp(style.jaw, 0.0f, 1.0f) * 0.5f * d; // pixels the lower face drops
	const bool stretched = style.warp == Warp::Wide || style.warp == Warp::Long;
	const float feather = 0.15f * d;
	const float reach = std::max(axes.width, axes.height) * (stretched ? 1.4f : 1.0f) + jaw + 3.0f * feather;
	const cv::Rect area = cv::Rect(cv::Point(cvFloor(center.x - reach), cvFloor(center.y - reach)),
		cv::Point(cvCeil(center.x + reach), cvCeil(center.y + reach))) & cv::Rect(0, 0, picture.cols, picture.rows);
	if (area.width < 4 || area.height < 4)
		return;

	cv::Mat mask = cv::Mat::zeros(area.size(), CV_8U);
	cv::ellipse(mask, cv::RotatedRect(center - cv::Point2f(area.tl()), axes * 2.0f * 0.92f, angle),
		cv::Scalar(255), cv::FILLED, cv::LINE_AA);
	const cv::Mat core = mask.clone(); // where the colors are measured: the face itself, not its fading edge
	cv::GaussianBlur(mask, mask, cv::Size(), feather);

	// The other face, lined up on this one.
	const cv::Point2f fromPoints[] = { fromFace.eyeLeft, fromFace.eyeRight, fromFace.nose, fromFace.mouth };
	const cv::Point2f ontoPoints[] = { onto.eyeLeft, onto.eyeRight, onto.nose, onto.mouth };
	cv::Matx23d warp = similarity(fromPoints, ontoPoints, 4);
	warp(0, 2) -= area.x;
	warp(1, 2) -= area.y;
	cv::Mat moved;
	cv::warpAffine(from, moved, warp, area.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
	cv::Mat target = picture(area);

	// Same light and skin tone as the face it covers: match the mean and
	// spread of each Lab channel, within limits so a dark scene stays a face.
	if (style.recolor)
	{
		cv::Mat movedLab, targetLab;
		cv::cvtColor(moved, movedLab, cv::COLOR_BGR2Lab);
		cv::cvtColor(target, targetLab, cv::COLOR_BGR2Lab);
		cv::Scalar movedMean, movedSpread, targetMean, targetSpread;
		cv::meanStdDev(movedLab, movedMean, movedSpread, core);
		cv::meanStdDev(targetLab, targetMean, targetSpread, core);
		std::vector<cv::Mat> channels;
		cv::split(movedLab, channels);
		for (int c = 0; c < 3; ++c)
		{
			const double gain = std::clamp(targetSpread[c] / std::max(1.0, movedSpread[c]), 0.7, 1.4);
			channels[c].convertTo(channels[c], CV_8U, gain, targetMean[c] - movedMean[c] * gain);
		}
		cv::merge(channels, movedLab);
		cv::cvtColor(movedLab, moved, cv::COLOR_Lab2BGR);
	}

	// Funhouse mirror and singing. Each pixel of the face (and of its mask)
	// is taken from elsewhere in the lined-up face: first undo the open jaw,
	// then the warp, so the jaw opens at the warped mouth.
	if (jaw >= 1.0f || style.warp != Warp::None)
	{
		const cv::Point2f offset(area.tl());
		const cv::Point2f mid = center - offset;
		const cv::Point2f mouth = onto.mouth - offset;
		const cv::Point2f eyeA = onto.eyeLeft - offset;
		const cv::Point2f eyeB = onto.eyeRight - offset;
		const float squeeze = 0.72f; // Wide and Long: the face is 1 / 0.72 times as wide or long
		// Where the mouth ends up after the warp; the jaw opens there.
		cv::Point2f lips = mouth;
		if (style.warp == Warp::Wide)
			lips = mid + across * ((mouth - mid).dot(across) / squeeze) + down * (mouth - mid).dot(down);
		else if (style.warp == Warp::Long)
			lips = mid + across * (mouth - mid).dot(across) + down * ((mouth - mid).dot(down) / squeeze);
		const float mouthHalf = 0.42f * d * (style.warp == Warp::Wide ? 1.0f / squeeze : 1.0f) *
			(style.warp == Warp::BigMouth ? 1.4f : 1.0f);
		const auto magnify = [](cv::Point2f q, cv::Point2f c, float radius, float strength) {
			const cv::Point2f v = q - c;
			const float r = length(v);
			if (r >= radius)
				return q;
			const float t = 1.0f - r / radius;
			// Near the middle, pixels come from even nearer it: all looks bigger.
			return c + v * (1.0f - strength * t * t);
		};

		cv::Mat mapX(area.size(), CV_32F), mapY(area.size(), CV_32F);
		cv::Mat hole = cv::Mat::zeros(area.size(), CV_8U);
		// Inside an open mouth: a row of upper teeth, then darkness.
		cv::Mat inner(area.size(), CV_8UC3, cv::Scalar(30, 15, 55));
		const cv::Vec3b teeth(225, 232, 238);
		for (int y = 0; y < area.height; ++y)
		{
			float* mx = mapX.ptr<float>(y);
			float* my = mapY.ptr<float>(y);
			unsigned char* inside = hole.ptr<unsigned char>(y);
			cv::Vec3b* paint = inner.ptr<cv::Vec3b>(y);
			for (int x = 0; x < area.width; ++x)
			{
				cv::Point2f q(static_cast<float>(x), static_cast<float>(y));
				if (jaw >= 1.0f)
				{
					const float v = (q - lips).dot(down);
					if (v > jaw)
						q -= down * jaw; // the chin, lowered
					else if (v > 0.0f)
					{
						const float a = (q - lips).dot(across) / mouthHalf;
						const float b = (v - jaw * 0.5f) / (jaw * 0.5f);
						if (a * a + b * b < 1.0f)
						{
							inside[x] = 255; // the open mouth
							if (v < jaw * 0.22f && std::fabs(a) < 0.8f)
								paint[x] = teeth;
						}
						q -= down * v;       // beside it, the cheeks stretch down
					}
				}
				switch (style.warp)
				{
				case Warp::BigEyes:
					q = magnify(magnify(q, eyeA, 0.6f * d, 0.5f), eyeB, 0.6f * d, 0.5f);
					break;
				case Warp::BigMouth:
					q = magnify(q, mouth, 0.85f * d, 0.55f);
					break;
				case Warp::Wide:
					q = mid + across * ((q - mid).dot(across) * squeeze) + down * (q - mid).dot(down);
					break;
				case Warp::Long:
					q = mid + across * (q - mid).dot(across) + down * ((q - mid).dot(down) * squeeze);
					break;
				default:
					break;
				}
				mx[x] = q.x;
				my[x] = q.y;
			}
		}
		cv::Mat face, faceMask;
		cv::remap(moved, face, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
		cv::remap(mask, faceMask, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));
		mask = faceMask;
		moved = face;
		if (jaw >= 1.0f)
		{
			// Soft enough not to look cut out, sharp enough to read as a mouth.
			cv::GaussianBlur(hole, hole, cv::Size(), std::max(0.8f, jaw * 0.04f));
			cv::GaussianBlur(inner, inner, cv::Size(), std::max(0.8f, jaw * 0.03f));
			cv::Mat open, closed;
			hole.convertTo(open, CV_32F, 1.0 / 255.0);
			cv::subtract(cv::Scalar(1.0), open, closed);
			cv::blendLinear(inner, face, open, closed, moved);
		}
	}

	cv::Mat weight, rest;
	mask.convertTo(weight, CV_32F, 1.0 / 255.0);
	cv::subtract(cv::Scalar(1.0), weight, rest);
	cv::Mat blended;
	cv::blendLinear(moved, target, weight, rest, blended);
	blended.copyTo(target);
}

std::vector<std::pair<int, int>> pairFaces(const std::vector<Face>& people, const std::vector<Face>& actors)
{
	const auto biggestFirst = [](const std::vector<Face>& faces) {
		std::vector<int> order;
		for (int i = 0; i < static_cast<int>(faces.size()); ++i)
			if (faces[i].eyeDistance() >= kMinEyeDistance)
				order.push_back(i);
		std::stable_sort(order.begin(), order.end(),
			[&faces](int a, int b) { return faces[a].eyeDistance() > faces[b].eyeDistance(); });
		return order;
	};
	const std::vector<int> p = biggestFirst(people);
	const std::vector<int> a = biggestFirst(actors);
	std::vector<std::pair<int, int>> pairs;
	for (std::size_t i = 0; i < std::min(p.size(), a.size()); ++i)
		pairs.emplace_back(p[i], a[i]);
	return pairs;
}

bool MoviePlayer::loadFaceTracker(const std::filesystem::path& model, std::string& error)
{
	return faceTracker_.load(model, error);
}

void MoviePlayer::open(const std::filesystem::path& file)
{
	close();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		frame_ = Frame();
	}
	stop_ = false;
	failed_ = false;
	paused_ = false;
	restart_ = false;
	seconds_ = 0.0;
	duration_ = 0.0;
	thread_ = std::thread(&MoviePlayer::run, this, file);
}

void MoviePlayer::close()
{
	stop_ = true;
	if (thread_.joinable())
		thread_.join();
}

MoviePlayer::Frame MoviePlayer::latest() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return frame_; // every frame gets a buffer of its own, so sharing is safe
}

cv::Mat MoviePlayer::firstFrame(const std::filesystem::path& file)
{
	cv::VideoCapture capture;
	cv::Mat frame;
	if (openVideo(capture, file))
		capture.read(frame);
	return frame;
}

void MoviePlayer::run(std::filesystem::path file)
{
	cv::VideoCapture capture;
	if (!openVideo(capture, file))
	{
		failed_ = true;
		return;
	}
	double fps = capture.get(cv::CAP_PROP_FPS);
	if (!(fps >= 1.0 && fps <= 240.0))
		fps = 30.0;
	const double frames = capture.get(cv::CAP_PROP_FRAME_COUNT);
	duration_ = frames > 0.0 ? frames / fps : 0.0;
	const Clock::duration interval = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / fps));

	FaceSmoother smoother;
	std::int64_t index = 0;
	Clock::time_point due = Clock::now();
	cv::Mat raw;
	while (!stop_)
	{
		// Back to the start: asked for, or the end of the video. Reopening
		// works for every format; seeking does not.
		const bool restart = restart_.exchange(false);
		if (restart && index > 0)
		{
			capture.release();
			if (!openVideo(capture, file))
			{
				failed_ = true;
				return;
			}
			index = 0;
			smoother.reset();
			due = Clock::now();
		}
		if (paused_)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			due = Clock::now();
			continue;
		}
		if (!capture.read(raw) || raw.empty())
		{
			if (index == 0) // not one frame: not a video OpenCV can read
			{
				failed_ = true;
				return;
			}
			restart_ = true;
			continue;
		}
		++index;
		seconds_ = index / fps;

		cv::Mat frame; // a new buffer each time: the last one may still be on screen
		if (raw.cols > kMaxWidth)
			cv::resize(raw, frame, cv::Size(), static_cast<double>(kMaxWidth) / raw.cols,
				static_cast<double>(kMaxWidth) / raw.cols, cv::INTER_AREA);
		else if (raw.type() == CV_8UC3)
			frame = raw.clone();
		else
			cv::cvtColor(raw, frame, raw.channels() == 4 ? cv::COLOR_BGRA2BGR : cv::COLOR_GRAY2BGR);
		if (frame.type() != CV_8UC3)
			cv::cvtColor(frame, frame, frame.channels() == 4 ? cv::COLOR_BGRA2BGR : cv::COLOR_GRAY2BGR);

		std::vector<Face> faces;
		if (faceTracker_.ready())
			faces = smoother.update(faceTracker_.detect(frame));
		{
			std::lock_guard<std::mutex> lock(mutex_);
			frame_.bgr = frame;
			frame_.faces = std::move(faces);
			++frame_.sequence;
		}

		// Keep the video's own pace; after a slow stretch, carry on from now
		// instead of rushing to catch up.
		due += interval;
		if (Clock::now() - due > std::chrono::milliseconds(250))
			due = Clock::now();
		while (!stop_ && !restart_ && Clock::now() < due)
			std::this_thread::sleep_for(std::min<Clock::duration>(due - Clock::now(), std::chrono::milliseconds(10)));
	}
}

} // namespace app
