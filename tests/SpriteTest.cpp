#include "Check.h"
#include "FrameBuffer.h"

#include "core/Sprite.h"

using namespace webcam;

namespace {

const Rgb kKey = { 255, 0, 0 };

// 4x2 picture: left half transparent, right half blue.
std::shared_ptr<const Image> halfKeyedPicture()
{
	BgrPicture pic(4, 2, kKey);
	for (int y = 0; y < 2; ++y)
	{
		pic.set(2, y, rgb(0, 0, 255));
		pic.set(3, y, rgb(0, 0, 255));
	}
	return Image::fromBgr24Keyed(&pic.bytes[0], pic.width, pic.height, pic.stride(), kKey);
}

} // namespace

TEST(transparent_pixels_leave_the_frame_alone)
{
	FrameBuffer fb(8, 4, yuv(50, 128, 128));
	Sprite sprite(halfKeyedPicture(), 2, 1);
	sprite.drawOnto(fb.frame);
	const Yuv blue = rgbToYuv(rgb(0, 0, 255));
	CHECK_EQ(50, fb.frame.pixel(2, 1).y);
	CHECK_EQ(50, fb.frame.pixel(3, 1).y);
	CHECK_EQ(blue.y, fb.frame.pixel(4, 1).y);
	CHECK_EQ(blue.u, fb.frame.pixel(4, 2).u);
	CHECK_EQ(50, fb.frame.pixel(4, 3).y);
}

TEST(drawing_clips_at_every_edge)
{
	FrameBuffer fb(4, 4, yuv(50, 128, 128));
	BgrPicture pic(6, 6, rgb(255, 255, 255));
	Sprite sprite(Image::fromBgr24(&pic.bytes[0], 6, 6, pic.stride()), -3, -3);
	sprite.drawOnto(fb.frame);
	CHECK_EQ(235, fb.frame.pixel(0, 0).y);
	CHECK_EQ(235, fb.frame.pixel(2, 2).y);
	CHECK_EQ(50, fb.frame.pixel(3, 3).y);
	sprite.moveTo(2, 2);
	sprite.drawOnto(fb.frame); // runs past the right and bottom edges
	CHECK_EQ(235, fb.frame.pixel(3, 3).y);
}

TEST(hidden_sprite_draws_nothing)
{
	FrameBuffer fb(4, 2, yuv(50, 128, 128));
	Sprite sprite(halfKeyedPicture(), 0, 0);
	sprite.hide();
	sprite.drawOnto(fb.frame);
	CHECK_EQ(50, fb.frame.pixel(3, 0).y);
}

TEST(overlap_counts_only_inner_blocks)
{
	BgrPicture pic(32, 32, rgb(255, 255, 255));
	Sprite sprite(Image::fromBgr24(&pic.bytes[0], 32, 32, pic.stride()), 8, 8);
	// The sprite covers blocks 1..4 in both directions; inner ones are 2..3.
	BlockMask edge(10, 10, 8);
	edge.set(1, 2, true);
	CHECK(!sprite.overlaps(edge));
	BlockMask inner(10, 10, 8);
	inner.set(3, 2, true);
	CHECK(sprite.overlaps(inner));
}

TEST(stretched_sample_covers_the_whole_picture)
{
	// Regression: a 1280x800 background used to show only its top-left 640x480.
	BgrPicture pic(4, 2, rgb(0, 0, 0));
	pic.set(3, 1, rgb(255, 255, 255));
	std::shared_ptr<const Image> image = Image::fromBgr24(&pic.bytes[0], 4, 2, pic.stride());
	CHECK_EQ(235, image->sampleStretched(7, 3, 8, 4).y);
	CHECK_EQ(16, image->sampleStretched(0, 0, 8, 4).y);
}
