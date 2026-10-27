#ifndef WEBCAM_APP_ASSETS_H
#define WEBCAM_APP_ASSETS_H

#include "core/ColorSpace.h"
#include "core/Sprite.h"

#include <opencv2/core.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace app {

// Pictures under assets/images, loaded with OpenCV.
class Assets
{
public:
	// Looks next to the executable, then in the working directory, then in the
	// source tree. Returns an empty path when none has assets/images.
	static std::filesystem::path locate();

	explicit Assets(std::filesystem::path root) : root_(std::move(root)) {}
	const std::filesystem::path& root() const { return root_; }

	// 8-bit BGR; empty if the file is missing or unreadable.
	cv::Mat bgr(const std::string& name) const;

	std::shared_ptr<const webcam::Image> picture(const std::string& name) const;
	std::shared_ptr<const webcam::Image> keyed(const std::string& name, webcam::Rgb transparent) const;

private:
	std::filesystem::path root_;
};

// Wraps continuous or strided 8-bit BGR pixels as a core Image.
std::shared_ptr<const webcam::Image> imageFromBgr(const cv::Mat& bgr);
std::shared_ptr<const webcam::Image> keyedImageFromBgr(const cv::Mat& bgr, webcam::Rgb transparent);

} // namespace app

#endif
