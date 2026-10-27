#include "Stickers.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace app {

namespace {

cv::Mat canvas(int width, int height)
{
	return cv::Mat(height, width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
}

cv::Mat drawSunglasses()
{
	cv::Mat art = canvas(420, 150);
	const cv::Scalar frame(20, 20, 20, 255), lens(45, 30, 25, 235), shine(255, 255, 255, 140);
	for (int side = 0; side < 2; ++side)
	{
		const cv::Point center(side == 0 ? 112 : 308, 78);
		cv::ellipse(art, center, cv::Size(92, 62), 0, 0, 360, lens, cv::FILLED, cv::LINE_AA);
		cv::ellipse(art, center, cv::Size(92, 62), 0, 0, 360, frame, 10, cv::LINE_AA);
		cv::ellipse(art, center + cv::Point(-34, -22), cv::Size(26, 12), -20, 0, 360, shine, cv::FILLED, cv::LINE_AA);
	}
	cv::ellipse(art, cv::Point(210, 70), cv::Size(28, 18), 0, 200, 340, frame, 10, cv::LINE_AA);
	cv::line(art, cv::Point(18, 60), cv::Point(2, 50), frame, 10, cv::LINE_AA);
	cv::line(art, cv::Point(402, 60), cv::Point(418, 50), frame, 10, cv::LINE_AA);
	return art;
}

cv::Mat drawPartyHat()
{
	cv::Mat art = canvas(260, 330);
	const std::vector<cv::Point> cone = { cv::Point(130, 40), cv::Point(240, 310), cv::Point(20, 310) };
	cv::fillConvexPoly(art, cone, cv::Scalar(200, 90, 230, 255), cv::LINE_AA);
	// Stripes, clipped to the cone by drawing only where the cone is.
	cv::Mat stripes = canvas(260, 330);
	for (int y = 60; y < 330; y += 54)
		cv::line(stripes, cv::Point(0, y + 40), cv::Point(260, y - 40), cv::Scalar(60, 220, 255, 255), 16, cv::LINE_AA);
	cv::Mat mask(330, 260, CV_8U, cv::Scalar(0));
	cv::fillConvexPoly(mask, cone, cv::Scalar(255), cv::LINE_AA);
	cv::Mat stripeAlpha;
	cv::extractChannel(stripes, stripeAlpha, 3);
	stripes.copyTo(art, mask & (stripeAlpha > 0));
	cv::ellipse(art, cv::Point(130, 310), cv::Size(115, 16), 0, 0, 360, cv::Scalar(70, 200, 90, 255), cv::FILLED, cv::LINE_AA);
	cv::circle(art, cv::Point(130, 36), 30, cv::Scalar(80, 230, 255, 255), cv::FILLED, cv::LINE_AA);
	return art;
}

cv::Mat drawMustache()
{
	cv::Mat art = canvas(320, 120);
	const cv::Scalar hair(30, 45, 70, 255);
	for (int side = -1; side <= 1; side += 2)
	{
		std::vector<cv::Point> curl;
		for (int i = 0; i <= 40; ++i)
		{
			const double t = i / 40.0;
			const double x = 160 + side * (10 + 140 * t);
			const double y = 50 + 28 * std::sin(t * CV_PI) - 34 * t * t;
			curl.push_back(cv::Point(static_cast<int>(x), static_cast<int>(y)));
		}
		for (int i = 40; i >= 0; --i)
		{
			const double t = i / 40.0;
			const double x = 160 + side * (10 + 140 * t);
			const double y = 50 + 28 * std::sin(t * CV_PI) - 34 * t * t - (36 * (1 - t) + 6);
			curl.push_back(cv::Point(static_cast<int>(x), static_cast<int>(y)));
		}
		cv::fillPoly(art, std::vector<std::vector<cv::Point>>{ curl }, hair, cv::LINE_AA);
		cv::circle(art, cv::Point(160 + side * 150, 16), 12, hair, cv::FILLED, cv::LINE_AA);
	}
	return art;
}

cv::Mat drawCatEars()
{
	cv::Mat art = canvas(440, 200);
	for (int side = 0; side < 2; ++side)
	{
		const int cx = side == 0 ? 95 : 345;
		const int lean = side == 0 ? -25 : 25;
		const std::vector<cv::Point> outer = { cv::Point(cx - 80, 195), cv::Point(cx + lean, 10), cv::Point(cx + 80, 195) };
		const std::vector<cv::Point> inner = { cv::Point(cx - 45, 185), cv::Point(cx + lean * 2 / 3, 55), cv::Point(cx + 45, 185) };
		cv::fillConvexPoly(art, outer, cv::Scalar(55, 55, 60, 255), cv::LINE_AA);
		cv::fillConvexPoly(art, inner, cv::Scalar(190, 160, 250, 255), cv::LINE_AA);
	}
	return art;
}

cv::Mat drawCrown()
{
	cv::Mat art = canvas(380, 230);
	const std::vector<cv::Point> shape = { cv::Point(20, 215), cv::Point(10, 60), cv::Point(95, 140), cv::Point(190, 20),
		cv::Point(285, 140), cv::Point(370, 60), cv::Point(360, 215) };
	cv::fillPoly(art, std::vector<std::vector<cv::Point>>{ shape }, cv::Scalar(40, 200, 250, 255), cv::LINE_AA);
	cv::polylines(art, std::vector<std::vector<cv::Point>>{ shape }, true, cv::Scalar(20, 130, 200, 255), 6, cv::LINE_AA);
	cv::rectangle(art, cv::Rect(20, 180, 340, 30), cv::Scalar(30, 160, 220, 255), cv::FILLED);
	const cv::Scalar jewels[] = { cv::Scalar(60, 60, 230, 255), cv::Scalar(230, 120, 60, 255), cv::Scalar(90, 200, 90, 255) };
	for (int i = 0; i < 3; ++i)
		cv::circle(art, cv::Point(95 + i * 95, 195), 14, jewels[i], cv::FILLED, cv::LINE_AA);
	for (const cv::Point& tip : { cv::Point(10, 60), cv::Point(190, 20), cv::Point(370, 60) })
		cv::circle(art, tip, 14, cv::Scalar(240, 240, 255, 255), cv::FILLED, cv::LINE_AA);
	return art;
}

} // namespace

void overlayBgra(cv::Mat& bgr, const cv::Mat& bgra, cv::Point2f center, float width, float degrees)
{
	if (bgra.empty() || bgra.type() != CV_8UC4 || width < 2.0f)
		return;
	const float scale = width / bgra.cols;
	// Where the art lands: the rotated, scaled rectangle's bounding box.
	cv::Mat m = cv::getRotationMatrix2D(cv::Point2f(bgra.cols * 0.5f, bgra.rows * 0.5f), -degrees, scale);
	m.at<double>(0, 2) += center.x - bgra.cols * 0.5;
	m.at<double>(1, 2) += center.y - bgra.rows * 0.5;
	std::vector<cv::Point2f> corners = { cv::Point2f(0, 0), cv::Point2f(static_cast<float>(bgra.cols), 0),
		cv::Point2f(static_cast<float>(bgra.cols), static_cast<float>(bgra.rows)), cv::Point2f(0, static_cast<float>(bgra.rows)) };
	cv::transform(corners, corners, m);
	const cv::Rect box = cv::boundingRect(corners) & cv::Rect(0, 0, bgr.cols, bgr.rows);
	if (box.area() <= 0)
		return;
	m.at<double>(0, 2) -= box.x;
	m.at<double>(1, 2) -= box.y;
	cv::Mat patch;
	cv::warpAffine(bgra, patch, m, box.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));

	cv::Mat roi = bgr(box);
	for (int y = 0; y < box.height; ++y)
	{
		const cv::Vec4b* src = patch.ptr<cv::Vec4b>(y);
		cv::Vec3b* dst = roi.ptr<cv::Vec3b>(y);
		for (int x = 0; x < box.width; ++x)
		{
			const int a = src[x][3];
			if (a == 0)
				continue;
			for (int c = 0; c < 3; ++c)
				dst[x][c] = static_cast<uchar>((src[x][c] * a + dst[x][c] * (255 - a) + 127) / 255);
		}
	}
}

FaceStickers::FaceStickers()
{
	art_[kSunglasses] = drawSunglasses();
	art_[kPartyHat] = drawPartyHat();
	art_[kMustache] = drawMustache();
	art_[kCatEars] = drawCatEars();
	art_[kCrown] = drawCrown();
}

const char* FaceStickers::name(Kind kind)
{
	switch (kind)
	{
	case kSunglasses: return "墨鏡";
	case kPartyHat: return "派對帽";
	case kMustache: return "鬍子";
	case kCatEars: return "貓耳";
	case kCrown: return "皇冠";
	default: return "";
	}
}

void FaceStickers::draw(cv::Mat& bgr, const std::vector<Face>& faces, Kind kind) const
{
	if (kind < 0 || kind >= kCount)
		return;
	const cv::Mat& art = art_[kind];
	for (const Face& face : faces)
	{
		const float d = face.eyeDistance();
		if (d < 4.0f)
			continue;
		const float tilt = face.tiltDegrees();
		const float radians = static_cast<float>(tilt * CV_PI / 180.0);
		// "Up" from the face's point of view.
		const cv::Point2f up(std::sin(radians), -std::cos(radians));
		const cv::Point2f eyes = face.eyesCenter();
		switch (kind)
		{
		case kSunglasses:
			overlayBgra(bgr, art, eyes, d * 2.4f, tilt);
			break;
		case kMustache:
			overlayBgra(bgr, art, face.nose + (face.mouth - face.nose) * 0.55f, d * 1.35f, tilt);
			break;
		case kPartyHat:
		{
			const float width = d * 1.9f;
			const float height = width * art.rows / art.cols;
			overlayBgra(bgr, art, eyes + up * (d * 1.15f + height * 0.5f), width, tilt);
			break;
		}
		case kCatEars:
		{
			const float width = d * 3.0f;
			const float height = width * art.rows / art.cols;
			overlayBgra(bgr, art, eyes + up * (d * 1.35f + height * 0.5f), width, tilt);
			break;
		}
		case kCrown:
		{
			const float width = d * 2.3f;
			const float height = width * art.rows / art.cols;
			overlayBgra(bgr, art, eyes + up * (d * 1.3f + height * 0.5f), width, tilt);
			break;
		}
		default:
			break;
		}
	}
}

} // namespace app
