#ifndef WEBCAM_APP_STYLE_H
#define WEBCAM_APP_STYLE_H

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace app {

enum class Look { None, Beauty, Cartoon, Sketch, Vintage };
enum class FrameStyle { None, Simple, Polaroid, Film };

struct Adjustments
{
	int brightness = 0;      // -100..100
	float contrast = 1.0f;   // 0.5..2
	float saturation = 1.0f; // 0..2
	bool vignette = false;

	bool any() const { return brightness != 0 || contrast != 1.0f || saturation != 1.0f || vignette; }
};

// Picture-wide looks, frames, and compositing on 8-bit BGR pictures. Keeps a
// few buffers between frames, so use one instance per stream of pictures.
class Styler
{
public:
	// The heavy filters (beauty, cartoon, background blur) run through
	// OpenCV's OpenCL path when a GPU is available; off or without OpenCL they
	// run on the CPU. Returns the device name, or empty when there is none.
	static std::string gpuName();
	void setUseGpu(bool on) { useGpu_ = on; }
	bool useGpu() const { return useGpu_; }

	void applyLook(cv::Mat& bgr, Look look);
	void applyAdjustments(cv::Mat& bgr, const Adjustments& adjust);

	// Draws a frame and/or a date stamp inside the picture (same size out).
	void applyFrame(cv::Mat& bgr, FrameStyle style, bool date);

	// Keeps the person (alpha 255) and replaces the rest with `background`
	// stretched to the picture; or blurs the rest instead.
	void replaceBackground(cv::Mat& bgr, const cv::Mat& alpha, const cv::Mat& background);
	void blurBackground(cv::Mat& bgr, const cv::Mat& alpha);

private:
	bool gpuOn() const;
	void blend(cv::Mat& bgr, const cv::Mat& alpha, const cv::Mat& behind);

	cv::Mat half_, smooth_, edges_, work_, vignette_, alphaF_, stretched_;
	cv::Size vignetteSize_;
	bool useGpu_ = true;
};

// Four pictures in a 2 x 2 grid on white, with a strip for the date below.
cv::Mat makeCollage(const std::vector<cv::Mat>& shots, bool date);

// "2026.10.27", as old cameras stamped it.
std::string dateStamp();

} // namespace app

#endif
