#include "Assets.h"

#include "Platform.h"

#include <opencv2/imgcodecs.hpp>

#include <vector>

namespace app {

std::filesystem::path Assets::locate()
{
	std::vector<std::filesystem::path> candidates;
	const std::filesystem::path exeDir = executableDir();
	if (!exeDir.empty())
	{
		candidates.push_back(exeDir / "assets");
		candidates.push_back(exeDir / ".." / "Resources" / "assets"); // macOS app bundle
	}
	candidates.push_back(std::filesystem::current_path() / "assets");
#ifdef WEBCAM_SOURCE_ASSETS
	candidates.push_back(pathFromUtf8(WEBCAM_SOURCE_ASSETS));
#endif
	for (const std::filesystem::path& dir : candidates)
	{
		std::error_code ec;
		if (std::filesystem::is_directory(dir / "images", ec))
			return dir;
	}
	return {};
}

cv::Mat Assets::bgr(const std::string& name) const
{
	std::vector<unsigned char> bytes;
	if (!readFile(root_ / "images" / name, bytes) || bytes.empty())
		return cv::Mat();
	return cv::imdecode(bytes, cv::IMREAD_COLOR);
}

std::shared_ptr<const webcam::Image> Assets::picture(const std::string& name) const
{
	return imageFromBgr(bgr(name));
}

std::shared_ptr<const webcam::Image> Assets::keyed(const std::string& name, webcam::Rgb transparent) const
{
	return keyedImageFromBgr(bgr(name), transparent);
}

std::shared_ptr<const webcam::Image> imageFromBgr(const cv::Mat& bgr)
{
	if (bgr.empty() || bgr.type() != CV_8UC3)
		return webcam::Image::fromBgr24(nullptr, 0, 0, 0);
	return webcam::Image::fromBgr24(bgr.data, bgr.cols, bgr.rows, bgr.step);
}

std::shared_ptr<const webcam::Image> keyedImageFromBgr(const cv::Mat& bgr, webcam::Rgb transparent)
{
	if (bgr.empty() || bgr.type() != CV_8UC3)
		return webcam::Image::fromBgr24(nullptr, 0, 0, 0);
	return webcam::Image::fromBgr24Keyed(bgr.data, bgr.cols, bgr.rows, bgr.step, transparent);
}

} // namespace app
