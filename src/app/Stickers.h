#ifndef WEBCAM_APP_STICKERS_H
#define WEBCAM_APP_STICKERS_H

#include "Vision.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace app {

// Draws an 8-bit BGRA picture onto a BGR picture: centered at `center`,
// `width` pixels wide, turned clockwise by `degrees`, blended by its alpha.
void overlayBgra(cv::Mat& bgr, const cv::Mat& bgra, cv::Point2f center, float width, float degrees = 0.0f);

// Stickers that follow faces. The art is drawn in code, so no image files.
class FaceStickers
{
public:
	enum Kind { kSunglasses, kPartyHat, kMustache, kCatEars, kCrown, kCount };

	FaceStickers();
	static const char* name(Kind kind);
	const cv::Mat& art(Kind kind) const { return art_[kind]; }

	// Puts the sticker on every face.
	void draw(cv::Mat& bgr, const std::vector<Face>& faces, Kind kind) const;

private:
	cv::Mat art_[kCount];
};

} // namespace app

#endif
