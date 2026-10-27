#include "Style.h"

#include <opencv2/core/ocl.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <ctime>

namespace app {

namespace {

const cv::Scalar kStampColor(40, 150, 255); // orange, like a film camera's date
const cv::Scalar kAccent(30, 148, 245);

int fontScaleFor(int width)
{
	return std::max(1, width / 640);
}

void putStamp(cv::Mat& bgr, cv::Point corner, const cv::Scalar& color, double scale)
{
	const std::string text = dateStamp();
	const int thickness = std::max(1, static_cast<int>(scale * 2));
	int baseline = 0;
	const cv::Size size = cv::getTextSize(text, cv::FONT_HERSHEY_DUPLEX, scale, thickness, &baseline);
	const cv::Point at(corner.x - size.width, corner.y);
	cv::putText(bgr, text, at + cv::Point(1, 1), cv::FONT_HERSHEY_DUPLEX, scale, cv::Scalar(0, 0, 0), thickness,
		cv::LINE_AA);
	cv::putText(bgr, text, at, cv::FONT_HERSHEY_DUPLEX, scale, color, thickness, cv::LINE_AA);
}

} // namespace

std::string Styler::gpuName()
{
	// Asking OpenCL the first time takes a while; the answer does not change.
	static const std::string name = cv::ocl::haveOpenCL() ? cv::ocl::Device::getDefault().name() : std::string();
	return name;
}

bool Styler::gpuOn() const
{
	return useGpu_ && cv::ocl::useOpenCL();
}

std::string dateStamp()
{
	const std::time_t now = std::time(nullptr);
	std::tm local{};
#if defined(_WIN32)
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	char text[16];
	std::strftime(text, sizeof(text), "%Y.%m.%d", &local);
	return text;
}

void Styler::applyLook(cv::Mat& bgr, Look look)
{
	switch (look)
	{
	case Look::None:
		return;

	case Look::Beauty:
	{
		// Edge-preserving smoothing at half size, blended back: softer skin,
		// edges and eyes stay sharp. The bilateral filter is ~7x faster on a GPU.
		if (gpuOn())
		{
			// Upload a copy rather than a view, since the result goes back into bgr.
			cv::UMat src, half, smooth, full, out;
			bgr.copyTo(src);
			cv::resize(src, half, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
			cv::bilateralFilter(half, smooth, 7, 40, 7);
			cv::resize(smooth, full, bgr.size(), 0, 0, cv::INTER_LINEAR);
			cv::addWeighted(src, 0.35, full, 0.65, 4.0, out);
			out.copyTo(bgr);
			return;
		}
		cv::resize(bgr, half_, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
		cv::bilateralFilter(half_, smooth_, 7, 40, 7);
		cv::resize(smooth_, work_, bgr.size(), 0, 0, cv::INTER_LINEAR);
		cv::addWeighted(bgr, 0.35, work_, 0.65, 4.0, bgr);
		return;
	}

	case Look::Cartoon:
	{
		// Flat colors from a quarter-size copy, ink lines from a half-size one.
		cv::resize(bgr, half_, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
		cv::Mat quarter;
		cv::resize(half_, quarter, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
		if (gpuOn())
		{
			cv::UMat q, s;
			quarter.copyTo(q);
			cv::bilateralFilter(q, s, 7, 60, 7);
			cv::bilateralFilter(s, q, 7, 60, 7);
			q.copyTo(quarter);
		}
		else
		{
			cv::bilateralFilter(quarter, smooth_, 7, 60, 7);
			cv::bilateralFilter(smooth_, quarter, 7, 60, 7);
		}
		// Fewer colors: round each channel to steps of 24.
		quarter.convertTo(quarter, CV_8U, 1.0 / 24.0);
		quarter.convertTo(quarter, CV_8U, 24.0, 12.0);
		cv::resize(quarter, work_, half_.size(), 0, 0, cv::INTER_LINEAR);
		cv::Mat gray;
		cv::cvtColor(half_, gray, cv::COLOR_BGR2GRAY);
		cv::medianBlur(gray, gray, 5);
		cv::adaptiveThreshold(gray, edges_, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 9, 5);
		cv::cvtColor(edges_, edges_, cv::COLOR_GRAY2BGR);
		cv::bitwise_and(work_, edges_, work_);
		cv::resize(work_, bgr, bgr.size(), 0, 0, cv::INTER_LINEAR);
		return;
	}

	case Look::Sketch:
	{
		// Pencil sketch: gray divided by its blurred negative ("color dodge").
		// The wide blur runs on a half-size copy.
		cv::Mat gray, inverted, small, blurred;
		cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
		cv::bitwise_not(gray, inverted);
		cv::resize(inverted, small, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
		cv::GaussianBlur(small, small, cv::Size(0, 0), std::max(1.5, bgr.cols / 240.0));
		cv::resize(small, blurred, gray.size(), 0, 0, cv::INTER_LINEAR);
		cv::bitwise_not(blurred, blurred);
		cv::divide(gray, blurred, gray, 256.0);
		cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
		return;
	}

	case Look::Vintage:
	{
		// Sepia (rows produce B, G, R from B, G, R), then a little fade.
		const cv::Matx33f sepia(0.131f, 0.534f, 0.272f, 0.168f, 0.686f, 0.349f, 0.189f, 0.769f, 0.393f);
		cv::transform(bgr, bgr, sepia);
		bgr.convertTo(bgr, CV_8U, 0.88, 18.0);
		return;
	}
	}
}

void Styler::applyAdjustments(cv::Mat& bgr, const Adjustments& adjust)
{
	if (adjust.contrast != 1.0f || adjust.brightness != 0)
	{
		// Contrast around mid-gray, then brightness.
		const double alpha = adjust.contrast;
		const double beta = 128.0 * (1.0 - alpha) + adjust.brightness;
		bgr.convertTo(bgr, CV_8U, alpha, beta);
	}
	if (adjust.saturation != 1.0f)
	{
		cv::Mat gray;
		cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
		cv::cvtColor(gray, gray, cv::COLOR_GRAY2BGR);
		cv::addWeighted(bgr, adjust.saturation, gray, 1.0 - adjust.saturation, 0.0, bgr);
	}
	if (adjust.vignette)
	{
		// Weights 0..255 per pixel, computed once per picture size; an
		// 8-bit multiply is far cheaper than going through floats.
		if (vignetteSize_ != bgr.size())
		{
			vignetteSize_ = bgr.size();
			vignette_.create(bgr.size(), CV_8UC3);
			const float cx = bgr.cols * 0.5f, cy = bgr.rows * 0.5f;
			const float reach = std::sqrt(cx * cx + cy * cy);
			for (int y = 0; y < bgr.rows; ++y)
				for (int x = 0; x < bgr.cols; ++x)
				{
					const float dx = (x - cx) / reach, dy = (y - cy) / reach;
					const float v = 1.0f - 0.55f * std::pow(std::min(1.0f, std::sqrt(dx * dx + dy * dy) * 1.15f), 2.2f);
					const uchar w = cv::saturate_cast<uchar>(v * 255.0f);
					vignette_.at<cv::Vec3b>(y, x) = cv::Vec3b(w, w, w);
				}
		}
		cv::multiply(bgr, vignette_, bgr, 1.0 / 255.0);
	}
}

void Styler::applyFrame(cv::Mat& bgr, FrameStyle style, bool date)
{
	const int w = bgr.cols, h = bgr.rows;
	const double textScale = 0.6 * fontScaleFor(w) + 0.2;
	switch (style)
	{
	case FrameStyle::None:
		if (date)
			putStamp(bgr, cv::Point(w - w / 30, h - h / 25), kStampColor, textScale);
		return;

	case FrameStyle::Simple:
	{
		const int t = std::max(6, w / 60);
		cv::rectangle(bgr, cv::Rect(0, 0, w, h), kAccent, t * 2);
		cv::rectangle(bgr, cv::Rect(t + t / 2, t + t / 2, w - 3 * t, h - 3 * t), cv::Scalar(255, 255, 255), 2,
			cv::LINE_AA);
		if (date)
			putStamp(bgr, cv::Point(w - 2 * t, h - 2 * t), kStampColor, textScale);
		return;
	}

	case FrameStyle::Polaroid:
	{
		// The picture shrinks into a white card with a wider bottom edge.
		const int side = std::max(8, w / 28);
		const int bottom = std::max(24, h / 6);
		cv::Mat card(bgr.size(), CV_8UC3, cv::Scalar(246, 246, 242));
		const cv::Rect inner(side, side, w - 2 * side, h - side - bottom);
		cv::Mat shrunk;
		cv::resize(bgr, shrunk, inner.size(), 0, 0, cv::INTER_AREA);
		shrunk.copyTo(card(inner));
		cv::rectangle(card, inner, cv::Scalar(200, 200, 200), 1);
		if (date)
			putStamp(card, cv::Point(w - side, h - bottom / 2 + static_cast<int>(10 * textScale)),
				cv::Scalar(90, 90, 90), textScale * 1.1);
		bgr = card;
		return;
	}

	case FrameStyle::Film:
	{
		// Black bars with sprocket holes at the top and bottom.
		const int bar = std::max(16, h / 9);
		cv::rectangle(bgr, cv::Rect(0, 0, w, bar), cv::Scalar(15, 15, 15), cv::FILLED);
		cv::rectangle(bgr, cv::Rect(0, h - bar, w, bar), cv::Scalar(15, 15, 15), cv::FILLED);
		const int hole = bar / 2;
		for (int x = hole / 2; x < w; x += hole * 2)
		{
			cv::rectangle(bgr, cv::Rect(x, bar / 4, hole, bar / 2), cv::Scalar(225, 225, 225), cv::FILLED, cv::LINE_AA);
			cv::rectangle(bgr, cv::Rect(x, h - bar + bar / 4, hole, bar / 2), cv::Scalar(225, 225, 225), cv::FILLED,
				cv::LINE_AA);
		}
		if (date)
			putStamp(bgr, cv::Point(w - w / 30, h - bar - h / 30), kStampColor, textScale);
		return;
	}
	}
}

void Styler::blend(cv::Mat& bgr, const cv::Mat& alpha, const cv::Mat& behind)
{
	// bgr = bgr * a + behind * (1 - a), with a from 0..255.
	alpha.convertTo(alphaF_, CV_32F, 1.0 / 255.0);
	cv::Mat inverse = 1.0f - alphaF_;
	cv::blendLinear(bgr, behind, alphaF_, inverse, bgr);
}

void Styler::replaceBackground(cv::Mat& bgr, const cv::Mat& alpha, const cv::Mat& background)
{
	if (background.empty() || alpha.size() != bgr.size())
		return;
	// Fill the picture without distorting the background: scale to cover, crop the middle.
	const double scale = std::max(static_cast<double>(bgr.cols) / background.cols,
		static_cast<double>(bgr.rows) / background.rows);
	cv::Mat covered;
	cv::resize(background, covered, cv::Size(), scale, scale, cv::INTER_LINEAR);
	const cv::Rect middle((covered.cols - bgr.cols) / 2, (covered.rows - bgr.rows) / 2, bgr.cols, bgr.rows);
	stretched_ = covered(middle);
	blend(bgr, alpha, stretched_);
}

void Styler::blurBackground(cv::Mat& bgr, const cv::Mat& alpha)
{
	if (alpha.size() != bgr.size())
		return;
	// Blur a quarter-size copy: a strong blur for a fraction of the cost.
	cv::Mat small;
	cv::resize(bgr, small, cv::Size(), 0.25, 0.25, cv::INTER_AREA);
	cv::GaussianBlur(small, small, cv::Size(0, 0), 4.0);
	cv::resize(small, work_, bgr.size(), 0, 0, cv::INTER_LINEAR);
	blend(bgr, alpha, work_);
}

cv::Mat makeCollage(const std::vector<cv::Mat>& shots, bool date)
{
	if (shots.empty())
		return cv::Mat();
	const int w = shots[0].cols / 2, h = shots[0].rows / 2;
	const int margin = std::max(8, w / 24);
	const int footer = std::max(28, h / 5);
	cv::Mat collage(2 * h + 3 * margin + footer, 2 * w + 3 * margin, CV_8UC3, cv::Scalar(250, 250, 250));
	for (std::size_t i = 0; i < shots.size() && i < 4; ++i)
	{
		const int col = static_cast<int>(i % 2), row = static_cast<int>(i / 2);
		const cv::Rect cell(margin + col * (w + margin), margin + row * (h + margin), w, h);
		cv::Mat small;
		cv::resize(shots[i], small, cell.size(), 0, 0, cv::INTER_AREA);
		small.copyTo(collage(cell));
	}
	if (date)
		putStamp(collage, cv::Point(collage.cols - margin, collage.rows - footer / 2 + 8), cv::Scalar(90, 90, 90),
			0.6 * fontScaleFor(collage.cols) + 0.3);
	return collage;
}

} // namespace app
