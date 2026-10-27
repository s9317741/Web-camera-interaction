#ifndef WEBCAM_CORE_YUY2FRAME_H
#define WEBCAM_CORE_YUY2FRAME_H

#include "ColorSpace.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace webcam {

// A view over a caller-owned YUY2 buffer (Y0 U Y1 V per pair of pixels,
// rows top to bottom, no row padding). All pixel coordinates are (x, y) with
// x in [0, width) and y in [0, height); accessors do not range-check.
class Yuy2Frame
{
public:
	// FOURCC 'YUY2' as stored in BITMAPINFOHEADER::biCompression.
	static const std::uint32_t kFourCC = 0x32595559u;

	static std::size_t bytesFor(int width, int height)
	{
		return static_cast<std::size_t>(width) * height * 2;
	}

	Yuy2Frame() : data_(0), width_(0), height_(0) {}

	// Points the view at a buffer. Fails (and leaves the view empty) when the
	// width is not a positive even number, the height is not positive, or the
	// buffer is smaller than width * height * 2 bytes.
	bool wrap(std::uint8_t* data, std::size_t size, int width, int height);

	bool empty() const { return data_ == 0; }
	int width() const { return width_; }
	int height() const { return height_; }
	std::uint8_t* data() { return data_; }
	const std::uint8_t* data() const { return data_; }
	std::size_t sizeBytes() const { return bytesFor(width_, height_); }

	Yuv pixel(int x, int y) const
	{
		const std::uint8_t* m = data_ + macroOffset(x, y);
		Yuv c = { m[(x & 1) * 2], m[1], m[3] };
		return c;
	}

	void setLuma(int x, int y, std::uint8_t value)
	{
		data_[macroOffset(x, y) + (x & 1) * 2] = value;
	}

	// U and V are shared by the two pixels of a macropixel.
	void setChroma(int x, int y, std::uint8_t u, std::uint8_t v)
	{
		std::uint8_t* m = data_ + macroOffset(x, y);
		m[1] = u;
		m[3] = v;
	}

	void setPixel(int x, int y, Yuv c)
	{
		setLuma(x, y, c.y);
		setChroma(x, y, c.u, c.v);
	}

	void mirrorHorizontally();

	// Writes the frame as 24-bit BGR rows, top to bottom.
	void copyToBgr24(std::uint8_t* dst, std::size_t dstStride) const;

private:
	std::size_t macroOffset(int x, int y) const
	{
		return (static_cast<std::size_t>(y) * width_ + (x & ~1)) * 2;
	}

	std::uint8_t* data_;
	int width_;
	int height_;
};

// A YUY2 frame that owns its pixels, e.g. a copy taken before effects run.
class Yuy2Image
{
public:
	void assign(const Yuy2Frame& source);
	const Yuy2Frame& frame() const { return frame_; }
	Yuy2Frame& frame() { return frame_; }

private:
	std::vector<std::uint8_t> bytes_;
	Yuy2Frame frame_;
};

} // namespace webcam

#endif
