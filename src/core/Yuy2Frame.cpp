#include "Yuy2Frame.h"

#include <algorithm>
#include <cstring>

namespace webcam {

const std::uint32_t Yuy2Frame::kFourCC;

bool Yuy2Frame::wrap(std::uint8_t* data, std::size_t size, int width, int height)
{
	data_ = 0;
	width_ = 0;
	height_ = 0;
	if (data == 0 || width <= 0 || height <= 0 || (width & 1) != 0)
		return false;
	if (size < bytesFor(width, height))
		return false;
	data_ = data;
	width_ = width;
	height_ = height;
	return true;
}

void Yuy2Frame::mirrorHorizontally()
{
	const int macros = width_ / 2;
	for (int y = 0; y < height_; ++y)
	{
		std::uint8_t* row = data_ + static_cast<std::size_t>(y) * width_ * 2;
		// Reverse the macropixel order; inside each macropixel Y0 and Y1
		// trade places so the pixel order is reversed too.
		for (int left = 0, right = macros - 1; left <= right; ++left, --right)
		{
			std::uint8_t* a = row + left * 4;
			std::uint8_t* b = row + right * 4;
			if (left == right)
			{
				std::swap(a[0], a[2]);
				break;
			}
			const std::uint8_t ay0 = a[0], au = a[1], ay1 = a[2], av = a[3];
			a[0] = b[2];
			a[1] = b[1];
			a[2] = b[0];
			a[3] = b[3];
			b[0] = ay1;
			b[1] = au;
			b[2] = ay0;
			b[3] = av;
		}
	}
}

void Yuy2Frame::copyToBgr24(std::uint8_t* dst, std::size_t dstStride) const
{
	for (int y = 0; y < height_; ++y)
	{
		std::uint8_t* out = dst + static_cast<std::size_t>(y) * dstStride;
		for (int x = 0; x < width_; ++x)
		{
			const Rgb c = yuvToRgb(pixel(x, y));
			out[x * 3 + 0] = c.b;
			out[x * 3 + 1] = c.g;
			out[x * 3 + 2] = c.r;
		}
	}
}

void Yuy2Frame::copyFromBgr24(const std::uint8_t* src, std::size_t srcStride)
{
	for (int y = 0; y < height_; ++y)
	{
		const std::uint8_t* in = src + static_cast<std::size_t>(y) * srcStride;
		std::uint8_t* out = data_ + static_cast<std::size_t>(y) * width_ * 2;
		for (int x = 0; x < width_; x += 2, in += 6, out += 4)
		{
			const Rgb left = { in[2], in[1], in[0] };
			const Rgb right = { in[5], in[4], in[3] };
			const Yuv a = rgbToYuv(left);
			const Yuv b = rgbToYuv(right);
			out[0] = a.y;
			out[1] = static_cast<std::uint8_t>((a.u + b.u + 1) / 2);
			out[2] = b.y;
			out[3] = static_cast<std::uint8_t>((a.v + b.v + 1) / 2);
		}
	}
}

void Yuy2Image::assign(const Yuy2Frame& source)
{
	bytes_.resize(source.sizeBytes());
	if (!bytes_.empty())
		std::memcpy(&bytes_[0], source.data(), bytes_.size());
	if (bytes_.empty())
		frame_ = Yuy2Frame();
	else
		frame_.wrap(&bytes_[0], bytes_.size(), source.width(), source.height());
}

} // namespace webcam
