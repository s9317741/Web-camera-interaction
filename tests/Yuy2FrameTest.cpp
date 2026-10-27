#include "Check.h"
#include "FrameBuffer.h"

#include "core/ColorSpace.h"
#include "core/Yuy2Frame.h"

#include <cstdlib>

using namespace webcam;

TEST(rgb_to_yuv_matches_bt601_studio_range)
{
	const Yuv white = rgbToYuv(rgb(255, 255, 255));
	CHECK_EQ(235, white.y);
	CHECK_EQ(128, white.u);
	CHECK_EQ(128, white.v);
	const Yuv black = rgbToYuv(rgb(0, 0, 0));
	CHECK_EQ(16, black.y);
	CHECK_EQ(128, black.u);
	CHECK_EQ(128, black.v);
}

TEST(yuv_round_trip_stays_close)
{
	const Rgb colors[] = { rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255), rgb(95, 40, 20) };
	for (int i = 0; i < 4; ++i)
	{
		const Rgb back = yuvToRgb(rgbToYuv(colors[i]));
		CHECK(std::abs(back.r - colors[i].r) <= 3);
		CHECK(std::abs(back.g - colors[i].g) <= 3);
		CHECK(std::abs(back.b - colors[i].b) <= 3);
	}
}

TEST(wrap_rejects_odd_width_and_short_buffer)
{
	std::vector<std::uint8_t> bytes(100);
	Yuy2Frame frame;
	CHECK(!frame.wrap(&bytes[0], bytes.size(), 5, 2));
	CHECK(frame.empty());
	CHECK(!frame.wrap(&bytes[0], bytes.size(), 10, 6)); // needs 120 bytes
	CHECK(frame.wrap(&bytes[0], bytes.size(), 10, 5));  // exactly 100
	CHECK_EQ(10, frame.width());
}

TEST(pixels_in_a_macropixel_share_chroma)
{
	FrameBuffer fb(4, 1, yuv(16, 128, 128));
	fb.frame.setPixel(1, 0, yuv(200, 90, 60));
	CHECK_EQ(16, fb.frame.pixel(0, 0).y);
	CHECK_EQ(200, fb.frame.pixel(1, 0).y);
	CHECK_EQ(90, fb.frame.pixel(0, 0).u);
	CHECK_EQ(60, fb.frame.pixel(0, 0).v);
	CHECK_EQ(128, fb.frame.pixel(2, 0).u);
}

TEST(mirror_reverses_every_pixel_of_every_row)
{
	// Widths 8 and 6 give an even and an odd number of macropixels.
	const int widths[] = { 8, 6 };
	for (int w = 0; w < 2; ++w)
	{
		const int width = widths[w];
		FrameBuffer fb(width, 3, yuv(0, 128, 128));
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < width; ++x)
				fb.frame.setLuma(x, y, static_cast<std::uint8_t>(10 * y + x));
		fb.frame.setChroma(0, 2, 50, 60);

		fb.frame.mirrorHorizontally();

		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < width; ++x)
				CHECK_EQ(10 * y + (width - 1 - x), fb.frame.pixel(x, y).y);
		CHECK_EQ(50, fb.frame.pixel(width - 1, 2).u);
		CHECK_EQ(60, fb.frame.pixel(width - 1, 2).v);
		CHECK_EQ(128, fb.frame.pixel(width - 1, 1).u);
	}
}

TEST(copy_from_bgr_round_trips_and_averages_chroma)
{
	// Pair 0: red and blue (chroma averaged); pair 1: two grays.
	const std::uint8_t bgr[] = { 0, 0, 255, 255, 0, 0, 128, 128, 128, 200, 200, 200 };
	FrameBuffer fb(4, 1, yuv(0, 0, 0));
	fb.frame.copyFromBgr24(bgr, sizeof(bgr));
	const Yuv red = rgbToYuv(rgb(255, 0, 0));
	const Yuv blue = rgbToYuv(rgb(0, 0, 255));
	CHECK_EQ(red.y, fb.frame.pixel(0, 0).y);
	CHECK_EQ(blue.y, fb.frame.pixel(1, 0).y);
	CHECK_EQ((red.u + blue.u + 1) / 2, fb.frame.pixel(0, 0).u);
	CHECK_EQ((red.v + blue.v + 1) / 2, fb.frame.pixel(1, 0).v);

	std::vector<std::uint8_t> back(sizeof(bgr));
	fb.frame.copyToBgr24(&back[0], sizeof(bgr));
	for (int i = 6; i < 12; ++i) // grays survive the round trip
		CHECK(std::abs(back[i] - bgr[i]) <= 2);
}

TEST(copy_to_bgr_converts_each_pixel)
{
	FrameBuffer fb(2, 2, rgbToYuv(rgb(0, 0, 0)));
	fb.frame.setLuma(1, 1, 235);
	std::vector<std::uint8_t> out(2 * 2 * 3);
	fb.frame.copyToBgr24(&out[0], 6);
	CHECK_EQ(0, out[0]);
	CHECK_EQ(255, out[9]);  // (1,1) blue
	CHECK_EQ(255, out[11]); // (1,1) red
}
