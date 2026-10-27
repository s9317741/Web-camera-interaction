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

// BT.601 studio-range conversion, integer approximation.
Yuv rgbToYuv(Rgb color);
Rgb yuvToRgb(Yuv color);

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
