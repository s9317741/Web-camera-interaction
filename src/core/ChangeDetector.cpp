#include "ChangeDetector.h"

#include <algorithm>

namespace webcam {

const int ChangeDetector::kBlockSize;
const int ChangeDetector::kDefaultThreshold;
const int ChangeDetector::kDefaultMaxDetectionWidth;

namespace {

// Noise floors, so a pixel that never moved while learning is not infinitely sensitive.
const float kMinVarY = 9.0f; // sigma 3
const float kMinVarC = 4.0f; // sigma 2 over U and V together

// Shadows keep at least this share of the expected brightness above black.
const float kShadowMinRatio = 0.5f;

// Only pixels this bright inform the brightness estimate; dark ones are noise.
const float kMinLumaForGain = 40.0f;

const int kMajorityRadius = 2; // 5x5 cells
const int kMaxLearningFrames = 1000; // keeps sums of squares within 32 bits
const int kMaxShift = 3; // cells of at most 8x8 pixels, so a cell never straddles a block

const float kDefaultAdaptationRate = 0.005f;

float median(std::vector<float>& values)
{
	std::vector<float>::iterator mid = values.begin() + values.size() / 2;
	std::nth_element(values.begin(), mid, values.end());
	return *mid;
}

int cellsFor(int pixels, int shift)
{
	return (pixels + (1 << shift) - 1) >> shift;
}

} // namespace

bool BlockMask::anyChanged() const
{
	return std::find(bits_.begin(), bits_.end(), 1) != bits_.end();
}

ChangeDetector::ChangeDetector()
	: width_(0), height_(0), shift_(0), frameWidth_(0), frameHeight_(0),
	  sumFrameWidth_(0), sumFrameHeight_(0), sumShift_(0), framesWanted_(0), framesSeen_(0),
	  learning_(false), threshold_(kDefaultThreshold),
	  adaptationRate_(kDefaultAdaptationRate),
	  maxDetectionWidth_(kDefaultMaxDetectionWidth), cellsWidth_(0), cellsHeight_(0)
{
}

int ChangeDetector::shiftFor(int frameWidth) const
{
	int shift = 0;
	while (shift < kMaxShift && cellsFor(frameWidth, shift) > maxDetectionWidth_)
		++shift;
	return shift;
}

void ChangeDetector::downsample(const Yuy2Frame& frame, int shift)
{
	const int w = frame.width();
	const int h = frame.height();
	cellsWidth_ = cellsFor(w, shift);
	cellsHeight_ = cellsFor(h, shift);
	cells_.resize(static_cast<std::size_t>(cellsWidth_) * cellsHeight_);

	if (shift == 0)
	{
		for (int y = 0; y < h; ++y)
			for (int x = 0; x < w; ++x)
				cells_[static_cast<std::size_t>(y) * w + x] = frame.pixel(x, y);
		return;
	}

	// Box average of each (2^shift)^2 cell, clipped at the right and bottom edges.
	// Cells are an even number of pixels wide, so both pixels of a YUY2 pair
	// (Y0 U Y1 V) fall into the same cell and can be read as one.
	const int s = 1 << shift;
	const std::uint8_t* data = frame.data();
	const std::size_t rowBytes = static_cast<std::size_t>(w) * 2;
	for (int cy = 0; cy < cellsHeight_; ++cy)
	{
		rowSums_.assign(static_cast<std::size_t>(cellsWidth_) * 3, 0);
		const int y0 = cy * s;
		const int y1 = std::min(h, y0 + s);
		for (int y = y0; y < y1; ++y)
		{
			const std::uint8_t* pair = data + static_cast<std::size_t>(y) * rowBytes;
			for (int x = 0; x < w; x += 2, pair += 4)
			{
				std::uint32_t* acc = &rowSums_[static_cast<std::size_t>(x >> shift) * 3];
				acc[0] += pair[0] + pair[2];
				acc[1] += pair[1] * 2u;
				acc[2] += pair[3] * 2u;
			}
		}
		for (int cx = 0; cx < cellsWidth_; ++cx)
		{
			const std::uint32_t count = static_cast<std::uint32_t>(
				(std::min(w, cx * s + s) - cx * s) * (y1 - y0));
			const std::uint32_t* acc = &rowSums_[static_cast<std::size_t>(cx) * 3];
			Yuv& out = cells_[static_cast<std::size_t>(cy) * cellsWidth_ + cx];
			out.y = static_cast<std::uint8_t>((acc[0] + count / 2) / count);
			out.u = static_cast<std::uint8_t>((acc[1] + count / 2) / count);
			out.v = static_cast<std::uint8_t>((acc[2] + count / 2) / count);
		}
	}
}

void ChangeDetector::startLearning(int frameCount)
{
	mean_.clear();
	varY_.clear();
	varC_.clear();
	width_ = 0;
	height_ = 0;
	frameWidth_ = 0;
	frameHeight_ = 0;
	sum_.clear();
	sumSq_.clear();
	sumFrameWidth_ = 0;
	sumFrameHeight_ = 0;
	framesSeen_ = 0;
	framesWanted_ = std::min(std::max(frameCount, 1), kMaxLearningFrames);
	learning_ = true;
	detection_ = Detection();
}

bool ChangeDetector::learn(const Yuy2Frame& frame)
{
	if (!learning_ || frame.empty())
		return false;

	if (frame.width() != sumFrameWidth_ || frame.height() != sumFrameHeight_)
	{
		// The cell size is fixed for the whole learning run.
		sumFrameWidth_ = frame.width();
		sumFrameHeight_ = frame.height();
		sumShift_ = shiftFor(frame.width());
		const std::size_t n = static_cast<std::size_t>(cellsFor(sumFrameWidth_, sumShift_)) *
			cellsFor(sumFrameHeight_, sumShift_) * 3;
		sum_.assign(n, 0);
		sumSq_.assign(n, 0);
		framesSeen_ = 0;
	}
	downsample(frame, sumShift_);

	for (std::size_t p = 0; p < cells_.size(); ++p)
	{
		const std::uint32_t v[3] = { cells_[p].y, cells_[p].u, cells_[p].v };
		for (int ch = 0; ch < 3; ++ch)
		{
			sum_[p * 3 + ch] += v[ch];
			sumSq_[p * 3 + ch] += v[ch] * v[ch];
		}
	}

	if (++framesSeen_ < framesWanted_)
		return false;

	frameWidth_ = sumFrameWidth_;
	frameHeight_ = sumFrameHeight_;
	shift_ = sumShift_;
	width_ = cellsWidth_;
	height_ = cellsHeight_;
	const std::size_t cells = cells_.size();
	mean_.resize(cells * 3);
	varY_.resize(cells);
	varC_.resize(cells);
	const float n = static_cast<float>(framesSeen_);
	for (std::size_t p = 0; p < cells; ++p)
	{
		float var[3];
		for (int ch = 0; ch < 3; ++ch)
		{
			const float m = sum_[p * 3 + ch] / n;
			mean_[p * 3 + ch] = m;
			var[ch] = std::max(0.0f, sumSq_[p * 3 + ch] / n - m * m);
		}
		varY_[p] = var[0];
		varC_[p] = var[1] + var[2];
	}
	sum_.clear();
	sumSq_.clear();
	learning_ = false;
	return true;
}

bool ChangeDetector::hasBackgroundFor(const Yuy2Frame& frame) const
{
	return hasBackground() && frame.width() == frameWidth_ && frame.height() == frameHeight_;
}

Yuv ChangeDetector::backgroundAt(int x, int y) const
{
	const float* m = &mean_[(static_cast<std::size_t>(y >> shift_) * width_ + (x >> shift_)) * 3];
	Yuv c = { clampToByte(static_cast<int>(m[0] + 0.5f)), clampToByte(static_cast<int>(m[1] + 0.5f)),
		clampToByte(static_cast<int>(m[2] + 0.5f)) };
	return c;
}

void ChangeDetector::mirrorBackground()
{
	// Exact when the frame width is a multiple of the cell size, which holds
	// for common video sizes; otherwise off by less than one cell.
	for (int y = 0; y < height_; ++y)
	{
		const std::size_t row = static_cast<std::size_t>(y) * width_;
		for (int left = 0, right = width_ - 1; left < right; ++left, --right)
		{
			for (int ch = 0; ch < 3; ++ch)
				std::swap(mean_[(row + left) * 3 + ch], mean_[(row + right) * 3 + ch]);
			std::swap(varY_[row + left], varY_[row + right]);
			std::swap(varC_[row + left], varC_[row + right]);
		}
	}
}

const Detection& ChangeDetector::observe(const Yuy2Frame& frame)
{
	Detection& d = detection_;
	d.width_ = frame.width();
	d.height_ = frame.height();
	d.valid_ = hasBackgroundFor(frame) && !learning_;
	d.shift_ = d.valid_ ? shift_ : shiftFor(frame.width());
	d.gridWidth_ = cellsFor(d.width_, d.shift_);
	d.gain_ = 1.0f;
	d.classes_.assign(static_cast<std::size_t>(d.gridWidth_) * cellsFor(d.height_, d.shift_),
		Detection::kBackground);
	d.blocks_ = BlockMask((d.width_ + kBlockSize - 1) / kBlockSize,
		(d.height_ + kBlockSize - 1) / kBlockSize, kBlockSize);
	if (!d.valid_)
		return d;

	downsample(frame, shift_);
	float gain, offsetU, offsetV;
	estimateGlobalShift(gain, offsetU, offsetV);
	d.gain_ = gain;
	classify(gain, offsetU, offsetV);
	smoothForeground();
	markBlocks();
	if (adaptationRate_ > 0.0f)
		adapt(gain, offsetU, offsetV);
	return d;
}

void ChangeDetector::estimateGlobalShift(float& gain, float& offsetU, float& offsetV) const
{
	// Medians over a sparse grid: robust while the foreground covers less than half.
	std::vector<float> ratios, du, dv;
	const int step = 4;
	for (int y = 0; y < height_; y += step)
		for (int x = 0; x < width_; x += step)
		{
			const std::size_t p = static_cast<std::size_t>(y) * width_ + x;
			const float* m = &mean_[p * 3];
			const Yuv c = cells_[p];
			if (m[0] >= kMinLumaForGain)
				ratios.push_back((c.y - 16.0f) / (m[0] - 16.0f));
			du.push_back(c.u - m[1]);
			dv.push_back(c.v - m[2]);
		}
	gain = ratios.size() >= 16 ? std::min(std::max(median(ratios), 0.5f), 2.0f) : 1.0f;
	offsetU = du.empty() ? 0.0f : std::min(std::max(median(du), -40.0f), 40.0f);
	offsetV = dv.empty() ? 0.0f : std::min(std::max(median(dv), -40.0f), 40.0f);
}

void ChangeDetector::classify(float gain, float offsetU, float offsetV)
{
	const float k2 = static_cast<float>(threshold_) * threshold_;
	rawForeground_.assign(cells_.size(), 0);
	std::vector<std::uint8_t>& classes = detection_.classes_;
	for (std::size_t p = 0; p < cells_.size(); ++p)
	{
		const float* m = &mean_[p * 3];
		const Yuv c = cells_[p];
		const float expectedY = 16.0f + gain * (m[0] - 16.0f);
		const float dy = c.y - expectedY;
		const float du = c.u - m[1] - offsetU;
		const float dv = c.v - m[2] - offsetV;
		const float zY = dy * dy / std::max(varY_[p], kMinVarY);
		const float zC = (du * du + dv * dv) / std::max(varC_[p], kMinVarC);
		if (zY + zC <= k2)
			continue;
		if (dy < 0.0f && zC <= k2 && (c.y - 16.0f) >= kShadowMinRatio * (expectedY - 16.0f))
			classes[p] = Detection::kShadow;
		else
			rawForeground_[p] = 1;
	}
}

void ChangeDetector::smoothForeground()
{
	// Majority vote over a (2r+1)^2 window, using a summed-area table.
	const int w = width_;
	const int h = height_;
	const std::size_t stride = static_cast<std::size_t>(w) + 1;
	integral_.assign(stride * (h + 1), 0);
	for (int y = 0; y < h; ++y)
	{
		std::uint32_t rowSum = 0;
		for (int x = 0; x < w; ++x)
		{
			rowSum += rawForeground_[static_cast<std::size_t>(y) * w + x];
			integral_[(y + 1) * stride + x + 1] = integral_[y * stride + x + 1] + rowSum;
		}
	}

	std::vector<std::uint8_t>& classes = detection_.classes_;
	const int r = kMajorityRadius;
	for (int y = 0; y < h; ++y)
	{
		const int y0 = std::max(0, y - r), y1 = std::min(h, y + r + 1);
		for (int x = 0; x < w; ++x)
		{
			const int x0 = std::max(0, x - r), x1 = std::min(w, x + r + 1);
			const std::uint32_t count = integral_[y1 * stride + x1] - integral_[y0 * stride + x1] -
				integral_[y1 * stride + x0] + integral_[y0 * stride + x0];
			const std::uint32_t area = static_cast<std::uint32_t>((y1 - y0) * (x1 - x0));
			std::uint8_t& cls = classes[static_cast<std::size_t>(y) * w + x];
			if (count * 2 > area)
				cls = Detection::kForeground;
			else if (cls == Detection::kForeground)
				cls = Detection::kBackground;
		}
	}
}

void ChangeDetector::markBlocks()
{
	// A cell is at most 8x8 pixels and aligned to a power of two, so it lies
	// inside exactly one block; it counts for the frame pixels it covers.
	BlockMask& blocks = detection_.blocks_;
	const int cols = blocks.cols();
	const int s = 1 << shift_;
	blockCounts_.assign(static_cast<std::size_t>(cols) * blocks.rows(), 0);
	for (int cy = 0; cy < height_; ++cy)
	{
		const int coveredH = std::min(s, frameHeight_ - cy * s);
		int* row = &blockCounts_[static_cast<std::size_t>((cy << shift_) / kBlockSize) * cols];
		for (int cx = 0; cx < width_; ++cx)
			if (detection_.classes_[static_cast<std::size_t>(cy) * width_ + cx] == Detection::kForeground)
				row[(cx << shift_) / kBlockSize] += std::min(s, frameWidth_ - cx * s) * coveredH;
	}
	for (int r = 0; r < blocks.rows(); ++r)
		for (int c = 0; c < cols; ++c)
		{
			const int bw = std::min(kBlockSize, frameWidth_ - c * kBlockSize);
			const int bh = std::min(kBlockSize, frameHeight_ - r * kBlockSize);
			blocks.set(c, r, blockCounts_[static_cast<std::size_t>(r) * cols + c] * 2 > bw * bh);
		}
}

void ChangeDetector::adapt(float gain, float offsetU, float offsetV)
{
	// Only blocks with no foreground or shadow at all, so a person standing
	// still is never absorbed through the gaps in their mask. The frame is
	// mapped back to the exposure the model was learned at, so the global
	// shift stays the job of the gain and offsets.
	const int cols = detection_.blocks_.cols();
	const int rows = detection_.blocks_.rows();
	std::vector<std::uint8_t> quiet(static_cast<std::size_t>(cols) * rows, 1);
	for (int cy = 0; cy < height_; ++cy)
		for (int cx = 0; cx < width_; ++cx)
			if (detection_.classes_[static_cast<std::size_t>(cy) * width_ + cx] != Detection::kBackground)
				quiet[static_cast<std::size_t>((cy << shift_) / kBlockSize) * cols +
					(cx << shift_) / kBlockSize] = 0;

	const float a = adaptationRate_;
	for (int cy = 0; cy < height_; ++cy)
		for (int cx = 0; cx < width_; ++cx)
		{
			if (!quiet[static_cast<std::size_t>((cy << shift_) / kBlockSize) * cols +
					(cx << shift_) / kBlockSize])
				continue;
			const std::size_t p = static_cast<std::size_t>(cy) * width_ + cx;
			float* m = &mean_[p * 3];
			const Yuv c = cells_[p];
			const float dy = 16.0f + (c.y - 16.0f) / gain - m[0];
			const float du = c.u - offsetU - m[1];
			const float dv = c.v - offsetV - m[2];
			m[0] += a * dy;
			m[1] += a * du;
			m[2] += a * dv;
			varY_[p] += a * (dy * dy - varY_[p]);
			varC_[p] += a * (du * du + dv * dv - varC_[p]);
		}
}

} // namespace webcam
