#ifndef WEBCAM_CORE_COLORSPACE_H
#define WEBCAM_CORE_COLORSPACE_H

#include <cstdint>

namespace webcam {

struct Rgb
{
	std::uint8_t r, g, b;
};

struct Yuv
{
	std::uint8_t y, u, v;
};

inline bool operator==(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
inline bool operator!=(Rgb a, Rgb b) { return !(a == b); }
inline bool operator==(Yuv a, Yuv b) { return a.y == b.y && a.u == b.u && a.v == b.v; }
inline bool operator!=(Yuv a, Yuv b) { return !(a == b); }

inline std::uint8_t clampToByte(int value)
{
	return static_cast<std::uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

// BT.601 studio-range conversion, integer approximation. Inline because it
// runs for every pixel of every frame.
inline Yuv rgbToYuv(Rgb c)
{
	const int r = c.r, g = c.g, b = c.b;
	Yuv out;
	out.y = clampToByte(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
	out.u = clampToByte(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
	out.v = clampToByte(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
	return out;
}

inline Rgb yuvToRgb(Yuv c)
{
	const int cy = c.y - 16;
	const int d = c.u - 128;
	const int e = c.v - 128;
	Rgb out;
	out.r = clampToByte((298 * cy + 409 * e + 128) >> 8);
	out.g = clampToByte((298 * cy - 100 * d - 208 * e + 128) >> 8);
	out.b = clampToByte((298 * cy + 516 * d + 128) >> 8);
	return out;
}

// Inclusive Y/U/V box used to pick pixels by color.
struct YuvRange
{
	int yMin, yMax;
	int uMin, uMax;
	int vMin, vMax;

	bool contains(Yuv c) const
	{
		return c.y >= yMin && c.y <= yMax && c.u >= uMin && c.u <= uMax &&
			c.v >= vMin && c.v <= vMax;
	}
};

} // namespace webcam

#endif
