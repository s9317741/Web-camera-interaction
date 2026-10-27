#ifndef WEBCAM_CORE_SPRITE_H
#define WEBCAM_CORE_SPRITE_H

#include "ChangeDetector.h"
#include "ColorSpace.h"
#include "Yuy2Frame.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace webcam {

// A picture converted to YUV once, with an optional transparent color.
class Image
{
public:
	// pixels: 24-bit BGR rows, top to bottom, stride bytes apart.
	static std::shared_ptr<const Image> fromBgr24(const std::uint8_t* pixels,
		int width, int height, std::size_t stride);
	static std::shared_ptr<const Image> fromBgr24Keyed(const std::uint8_t* pixels,
		int width, int height, std::size_t stride, Rgb transparent);

	int width() const { return width_; }
	int height() const { return height_; }
	Yuv at(int x, int y) const { return pixels_[index(x, y)]; }
	bool opaque(int x, int y) const { return opaque_[index(x, y)] != 0; }

	// Nearest-neighbour sample when the image is stretched to targetWidth x targetHeight.
	Yuv sampleStretched(int x, int y, int targetWidth, int targetHeight) const;

private:
	Image() : width_(0), height_(0) {}
	std::size_t index(int x, int y) const
	{
		return static_cast<std::size_t>(y) * width_ + x;
	}
	static std::shared_ptr<const Image> build(const std::uint8_t* pixels,
		int width, int height, std::size_t stride, bool keyed, Rgb transparent);

	int width_;
	int height_;
	std::vector<Yuv> pixels_;
	std::vector<std::uint8_t> opaque_;
};

// An Image placed on the frame. Drawing clips to the frame edges.
class Sprite
{
public:
	Sprite() : x_(0), y_(0), visible_(true) {}
	explicit Sprite(std::shared_ptr<const Image> image, int x = 0, int y = 0)
		: image_(image), x_(x), y_(y), visible_(true) {}

	void setImage(std::shared_ptr<const Image> image) { image_ = image; }
	const std::shared_ptr<const Image>& image() const { return image_; }

	void moveTo(int x, int y) { x_ = x; y_ = y; }
	int x() const { return x_; }
	int y() const { return y_; }
	int width() const { return image_ ? image_->width() : 0; }
	int height() const { return image_ ? image_->height() : 0; }

	bool visible() const { return visible_; }
	void show() { visible_ = true; }
	void hide() { visible_ = false; }

	// Draws opaque pixels; does nothing when hidden or without an image.
	void drawOnto(Yuy2Frame& frame) const;

	// True when a changed block lies strictly inside the blocks the sprite
	// covers, so touching only its edge does not count.
	bool overlaps(const BlockMask& changes) const;

private:
	std::shared_ptr<const Image> image_;
	int x_;
	int y_;
	bool visible_;
};

} // namespace webcam

#endif
