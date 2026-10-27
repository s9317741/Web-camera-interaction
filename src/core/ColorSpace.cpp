#include "ColorSpace.h"

namespace webcam {

Yuv rgbToYuv(Rgb c)
{
	const int r = c.r, g = c.g, b = c.b;
	Yuv out;
	out.y = clampToByte(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
	out.u = clampToByte(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
	out.v = clampToByte(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
	return out;
}

Rgb yuvToRgb(Yuv c)
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

} // namespace webcam
