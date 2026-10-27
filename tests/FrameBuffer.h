#ifndef WEBCAM_TESTS_FRAMEBUFFER_H
#define WEBCAM_TESTS_FRAMEBUFFER_H

#include "core/Yuy2Frame.h"

#include <cstdint>
#include <vector>

// Owns a YUY2 buffer filled with one color, for building test frames.
struct FrameBuffer
{
	std::vector<std::uint8_t> bytes;
	webcam::Yuy2Frame frame;

	FrameBuffer(int width, int height, webcam::Yuv fill)
		: bytes(webcam::Yuy2Frame::bytesFor(width, height))
	{
		frame.wrap(&bytes[0], bytes.size(), width, height);
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x)
				frame.setPixel(x, y, fill);
	}

	// The view points into bytes, so a copy would dangle.
	FrameBuffer(const FrameBuffer&) = delete;
	FrameBuffer& operator=(const FrameBuffer&) = delete;

	void fillRect(int x0, int y0, int w, int h, webcam::Yuv c)
	{
		for (int y = y0; y < y0 + h; ++y)
			for (int x = x0; x < x0 + w; ++x)
				frame.setPixel(x, y, c);
	}
};

inline webcam::Yuv yuv(int y, int u, int v)
{
	webcam::Yuv c = { static_cast<std::uint8_t>(y), static_cast<std::uint8_t>(u),
		static_cast<std::uint8_t>(v) };
	return c;
}

// 24-bit BGR pixels, top to bottom, for building Images.
struct BgrPicture
{
	int width, height;
	std::vector<std::uint8_t> bytes;

	BgrPicture(int w, int h, webcam::Rgb fill)
		: width(w), height(h), bytes(static_cast<std::size_t>(w) * h * 3)
	{
		for (int y = 0; y < h; ++y)
			for (int x = 0; x < w; ++x)
				set(x, y, fill);
	}

	void set(int x, int y, webcam::Rgb c)
	{
		std::uint8_t* p = &bytes[(static_cast<std::size_t>(y) * width + x) * 3];
		p[0] = c.b;
		p[1] = c.g;
		p[2] = c.r;
	}

	std::size_t stride() const { return static_cast<std::size_t>(width) * 3; }
};

inline webcam::Rgb rgb(int r, int g, int b)
{
	webcam::Rgb c = { static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
		static_cast<std::uint8_t>(b) };
	return c;
}

#endif
