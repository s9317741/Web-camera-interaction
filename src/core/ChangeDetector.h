#ifndef WEBCAM_CORE_CHANGEDETECTOR_H
#define WEBCAM_CORE_CHANGEDETECTOR_H

#include "Yuy2Frame.h"

#include <cstdint>
#include <vector>

namespace webcam {

// Which blocks of a frame are mostly foreground.
// Out-of-range block coordinates read as unchanged.
class BlockMask
{
public:
	BlockMask() : cols_(0), rows_(0), blockSize_(0) {}
	BlockMask(int cols, int rows, int blockSize)
		: cols_(cols), rows_(rows), blockSize_(blockSize),
		  bits_(static_cast<std::size_t>(cols) * rows, 0) {}

	int cols() const { return cols_; }
	int rows() const { return rows_; }
	int blockSize() const { return blockSize_; }

	bool changed(int col, int row) const
	{
		if (col < 0 || row < 0 || col >= cols_ || row >= rows_)
			return false;
		return bits_[static_cast<std::size_t>(row) * cols_ + col] != 0;
	}

	void set(int col, int row, bool value)
	{
		bits_[static_cast<std::size_t>(row) * cols_ + col] = value ? 1 : 0;
	}

	bool anyChanged() const;

private:
	int cols_;
	int rows_;
	int blockSize_;
	std::vector<std::uint8_t> bits_;
};

// The result of comparing one frame with the background. Coordinates are
// frame pixels even when detection ran on a scaled-down copy.
class Detection
{
public:
	enum PixelClass
	{
		kBackground = 0,
		kShadow = 1,     // darker than the background but the same color
		kForeground = 2
	};

	Detection() : width_(0), height_(0), shift_(0), gridWidth_(0), valid_(false), gain_(1.0f) {}

	// False when there was no background of the frame's size to compare with;
	// then nothing is foreground.
	bool valid() const { return valid_; }
	int width() const { return width_; }
	int height() const { return height_; }

	// How many frame pixels, per side, one detection cell covers (1, 2, 4 or 8).
	int scale() const { return 1 << shift_; }

	// After clean-up: isolated pixels removed, small holes filled.
	bool foreground(int x, int y) const { return classAt(x, y) == kForeground; }
	bool shadow(int x, int y) const { return classAt(x, y) == kShadow; }
	PixelClass classAt(int x, int y) const
	{
		return static_cast<PixelClass>(
			classes_[static_cast<std::size_t>(y >> shift_) * gridWidth_ + (x >> shift_)]);
	}

	// A block is changed when more than half of its pixels are foreground.
	const BlockMask& blocks() const { return blocks_; }

	// Estimated camera brightness change since the background was learned.
	float gain() const { return gain_; }

private:
	friend class ChangeDetector;

	int width_;
	int height_;
	int shift_;
	int gridWidth_;
	bool valid_;
	float gain_;
	std::vector<std::uint8_t> classes_; // one per detection cell
	BlockMask blocks_;
};

// Learns a background (per-pixel mean and noise) from a few frames, then
// splits later frames into background, shadow, and foreground.
//
// Per pixel, the luma and chroma differences are measured in units of that
// pixel's own noise, after cancelling global brightness and color shifts from
// auto exposure and white balance. A darker pixel with unchanged color is a
// shadow, not foreground. The mask is then smoothed by a 5x5 majority vote.
// Background pixels far from any foreground slowly blend into the model so it
// follows gradual lighting changes.
//
// Wide frames are averaged down by a power of two (at most 8) until they are
// no wider than the maximum detection width, so 1080p runs on 960x540 cells.
// Results are still reported per frame pixel.
class ChangeDetector
{
public:
	static const int kBlockSize = 8;
	static const int kDefaultThreshold = 5;
	static const int kDefaultMaxDetectionWidth = 960;

	ChangeDetector();

	// Discards the current background and models the next frameCount frames.
	void startLearning(int frameCount);
	bool isLearning() const { return learning_; }

	// Feeds one frame while learning. Returns true on the frame that completes
	// the background. A frame of a different size restarts learning.
	bool learn(const Yuy2Frame& frame);

	bool hasBackground() const { return !mean_.empty(); }
	bool hasBackgroundFor(const Yuy2Frame& frame) const;
	int backgroundWidth() const { return frameWidth_; }
	int backgroundHeight() const { return frameHeight_; }
	// In frame coordinates; neighbouring pixels share a value when scaled down.
	Yuv backgroundAt(int x, int y) const;

	// Flips the learned background so it stays aligned with mirrored frames.
	void mirrorBackground();

	// A pixel is foreground when its difference exceeds this many times its noise.
	void setThreshold(int noiseMultiple) { threshold_ = noiseMultiple < 1 ? 1 : noiseMultiple; }
	int threshold() const { return threshold_; }

	// How fast background pixels follow the frame, per frame; 0 freezes the model.
	void setAdaptationRate(float rate) { adaptationRate_ = rate; }
	float adaptationRate() const { return adaptationRate_; }

	// Takes effect at the next startLearning().
	void setMaxDetectionWidth(int width) { maxDetectionWidth_ = width < 1 ? 1 : width; }
	int maxDetectionWidth() const { return maxDetectionWidth_; }

	// Classifies the frame and adapts the background. The result stays valid
	// until the next call.
	const Detection& observe(const Yuy2Frame& frame);
	const Detection& last() const { return detection_; }

private:
	int shiftFor(int frameWidth) const;
	void downsample(const Yuy2Frame& frame, int shift);
	void estimateGlobalShift(float& gain, float& offsetU, float& offsetV) const;
	void classify(float gain, float offsetU, float offsetV);
	void smoothForeground();
	void markBlocks();
	void adapt(float gain, float offsetU, float offsetV);

	// Model, one entry per detection cell.
	std::vector<float> mean_;   // Y, U, V interleaved
	std::vector<float> varY_;
	std::vector<float> varC_;   // variance of U plus variance of V
	int width_;                 // in cells
	int height_;
	int shift_;
	int frameWidth_;            // in frame pixels
	int frameHeight_;

	// Learning state.
	std::vector<std::uint32_t> sum_;   // Y, U, V
	std::vector<std::uint32_t> sumSq_; // Y, U, V
	int sumFrameWidth_;
	int sumFrameHeight_;
	int sumShift_;
	int framesWanted_;
	int framesSeen_;
	bool learning_;

	int threshold_;
	float adaptationRate_;
	int maxDetectionWidth_;

	// Per-frame scratch.
	std::vector<Yuv> cells_;
	int cellsWidth_;
	int cellsHeight_;
	std::vector<std::uint32_t> rowSums_;
	Detection detection_;
	std::vector<std::uint8_t> rawForeground_;
	std::vector<std::uint32_t> integral_;
	std::vector<int> blockCounts_;
};

} // namespace webcam

#endif
