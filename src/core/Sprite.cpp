#include "Sprite.h"

#include <algorithm>

namespace webcam {

std::shared_ptr<const Image> Image::fromBgr24(const std::uint8_t* pixels,
	int width, int height, std::size_t stride)
{
	Rgb none = { 0, 0, 0 };
	return build(pixels, width, height, stride, false, none);
}

std::shared_ptr<const Image> Image::fromBgr24Keyed(const std::uint8_t* pixels,
	int width, int height, std::size_t stride, Rgb transparent)
{
	return build(pixels, width, height, stride, true, transparent);
}

std::shared_ptr<const Image> Image::build(const std::uint8_t* pixels,
	int width, int height, std::size_t stride, bool keyed, Rgb transparent)
{
	std::shared_ptr<Image> image(new Image());
	if (pixels == 0 || width <= 0 || height <= 0)
		return image;
	image->width_ = width;
	image->height_ = height;
	image->pixels_.resize(static_cast<std::size_t>(width) * height);
	image->opaque_.resize(image->pixels_.size());
	for (int y = 0; y < height; ++y)
	{
		const std::uint8_t* row = pixels + static_cast<std::size_t>(y) * stride;
		for (int x = 0; x < width; ++x)
		{
			const Rgb c = { row[x * 3 + 2], row[x * 3 + 1], row[x * 3 + 0] };
			const std::size_t i = image->index(x, y);
			image->pixels_[i] = rgbToYuv(c);
			image->opaque_[i] = (keyed && c == transparent) ? 0 : 1;
		}
	}
	return image;
}

Yuv Image::sampleStretched(int x, int y, int targetWidth, int targetHeight) const
{
	const int sx = static_cast<int>(static_cast<long long>(x) * width_ / targetWidth);
	const int sy = static_cast<int>(static_cast<long long>(y) * height_ / targetHeight);
	return at(std::min(sx, width_ - 1), std::min(sy, height_ - 1));
}

void Sprite::drawOnto(Yuy2Frame& frame) const
{
	if (!visible_ || !image_ || image_->width() == 0)
		return;
	const int x0 = std::max(0, x_);
	const int y0 = std::max(0, y_);
	const int x1 = std::min(frame.width(), x_ + image_->width());
	const int y1 = std::min(frame.height(), y_ + image_->height());
	for (int y = y0; y < y1; ++y)
		for (int x = x0; x < x1; ++x)
		{
			const int ix = x - x_;
			const int iy = y - y_;
			if (image_->opaque(ix, iy))
				frame.setPixel(x, y, image_->at(ix, iy));
		}
}

bool Sprite::overlaps(const BlockMask& changes) const
{
	if (!image_ || image_->width() == 0 || changes.blockSize() == 0)
		return false;
	const int size = changes.blockSize();
	const int left = x_ / size;
	const int top = y_ / size;
	const int right = (x_ + image_->width() - 1) / size;
	const int bottom = (y_ + image_->height() - 1) / size;
	for (int row = top + 1; row < bottom; ++row)
		for (int col = left + 1; col < right; ++col)
			if (changes.changed(col, row))
				return true;
	return false;
}

} // namespace webcam
