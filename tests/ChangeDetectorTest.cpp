#include "Check.h"
#include "FrameBuffer.h"

#include "core/ChangeDetector.h"

using namespace webcam;

namespace {

void learnFrom(ChangeDetector& detector, const Yuy2Frame& frame, int frames)
{
	detector.startLearning(frames);
	for (int i = 0; i < frames; ++i)
		detector.learn(frame);
}

int countBlocks(const BlockMask& mask)
{
	int n = 0;
	for (int r = 0; r < mask.rows(); ++r)
		for (int c = 0; c < mask.cols(); ++c)
			n += mask.changed(c, r) ? 1 : 0;
	return n;
}

int countForeground(const Detection& d)
{
	int n = 0;
	for (int y = 0; y < d.height(); ++y)
		for (int x = 0; x < d.width(); ++x)
			n += d.foreground(x, y) ? 1 : 0;
	return n;
}

} // namespace

TEST(nothing_is_foreground_before_a_background_exists)
{
	ChangeDetector detector;
	FrameBuffer fb(64, 48, yuv(200, 90, 90));
	const Detection& d = detector.observe(fb.frame);
	CHECK(!d.valid());
	CHECK_EQ(8, d.blocks().cols());
	CHECK_EQ(6, d.blocks().rows());
	CHECK(!d.blocks().anyChanged());
	CHECK_EQ(0, countForeground(d));
}

TEST(learning_reports_ready_on_the_last_frame)
{
	ChangeDetector detector;
	FrameBuffer fb(16, 16, yuv(100, 128, 128));
	detector.startLearning(3);
	CHECK(!detector.learn(fb.frame));
	CHECK(!detector.learn(fb.frame));
	CHECK(detector.learn(fb.frame));
	CHECK(!detector.isLearning());
	CHECK(detector.hasBackgroundFor(fb.frame));
}

TEST(background_is_the_average_of_learned_frames)
{
	ChangeDetector detector;
	FrameBuffer dark(4, 2, yuv(40, 100, 120));
	FrameBuffer light(4, 2, yuv(80, 140, 160));
	detector.startLearning(2);
	detector.learn(dark.frame);
	detector.learn(light.frame);
	CHECK_EQ(60, detector.backgroundAt(1, 1).y);
	CHECK_EQ(120, detector.backgroundAt(1, 1).u);
	CHECK_EQ(140, detector.backgroundAt(1, 1).v);
}

TEST(unchanged_scene_has_no_foreground)
{
	ChangeDetector detector;
	FrameBuffer fb(64, 48, yuv(100, 128, 128));
	learnFrom(detector, fb.frame, 2);
	const Detection& d = detector.observe(fb.frame);
	CHECK(d.valid());
	CHECK_EQ(0, countForeground(d));
}

TEST(object_marks_only_its_block)
{
	ChangeDetector detector;
	FrameBuffer scene(64, 48, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 2);
	scene.fillRect(16, 24, 8, 8, yuv(200, 128, 128)); // block (2, 3)
	const Detection& d = detector.observe(scene.frame);
	CHECK(d.blocks().changed(2, 3));
	CHECK_EQ(1, countBlocks(d.blocks()));
	CHECK(d.foreground(19, 27));
	CHECK(!d.foreground(40, 10));
}

TEST(chroma_only_change_is_foreground)
{
	// Regression: the old block mode stored the V difference in the U variable.
	ChangeDetector detector;
	FrameBuffer scene(32, 32, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 1);
	scene.fillRect(8, 8, 8, 8, yuv(100, 128, 160)); // only V moves
	const Detection& d = detector.observe(scene.frame);
	CHECK(d.foreground(11, 11));
	CHECK(d.blocks().changed(1, 1));
}

TEST(auto_exposure_brightening_everything_is_not_foreground)
{
	ChangeDetector detector;
	FrameBuffer scene(64, 48, yuv(100, 128, 128));
	scene.fillRect(0, 0, 32, 48, yuv(180, 110, 140));
	learnFrom(detector, scene.frame, 2);

	// The camera brightens the whole picture by 30% and shifts white balance.
	FrameBuffer brighter(64, 48, yuv(16 + 84 * 13 / 10, 134, 122));
	brighter.fillRect(0, 0, 32, 48, yuv(16 + 164 * 13 / 10, 116, 134));
	const Detection& d = detector.observe(brighter.frame);
	CHECK_EQ(0, countForeground(d));
	CHECK(d.gain() > 1.25f && d.gain() < 1.35f);
}

TEST(shadow_is_not_foreground)
{
	ChangeDetector detector;
	FrameBuffer scene(32, 32, yuv(160, 120, 135));
	learnFrom(detector, scene.frame, 1);
	// 40% darker, same color; kept under half the frame so the gain stays 1.
	scene.fillRect(0, 0, 12, 32, yuv(16 + 144 * 6 / 10, 120, 135));
	const Detection& d = detector.observe(scene.frame);
	CHECK(d.shadow(5, 5));
	CHECK(!d.foreground(5, 5));
	CHECK(!d.blocks().anyChanged());
}

TEST(very_dark_or_differently_colored_object_is_not_a_shadow)
{
	ChangeDetector detector;
	FrameBuffer scene(32, 32, yuv(160, 120, 135));
	learnFrom(detector, scene.frame, 1);
	scene.fillRect(0, 0, 16, 16, yuv(30, 120, 135));   // far too dark for a shadow
	scene.fillRect(16, 16, 16, 16, yuv(110, 150, 110)); // darker and a different color
	const Detection& d = detector.observe(scene.frame);
	CHECK(d.foreground(5, 5));
	CHECK(d.foreground(24, 24));
}

TEST(noisy_pixels_need_a_bigger_change_than_quiet_ones)
{
	// Left half flickers by +-10 while learning; right half is still.
	ChangeDetector detector;
	FrameBuffer a(32, 16, yuv(100, 128, 128));
	FrameBuffer b(32, 16, yuv(100, 128, 128));
	a.fillRect(0, 0, 16, 16, yuv(90, 128, 128));
	b.fillRect(0, 0, 16, 16, yuv(110, 128, 128));
	detector.startLearning(2);
	detector.learn(a.frame);
	detector.learn(b.frame);

	// The same +25 change everywhere (the median of the frame decides the
	// gain, so keep most of the frame at the background level).
	FrameBuffer now(32, 16, yuv(100, 128, 128));
	now.fillRect(4, 4, 8, 8, yuv(125, 128, 128));
	now.fillRect(20, 4, 8, 8, yuv(125, 128, 128));
	detector.setAdaptationRate(0.0f);
	const Detection& d = detector.observe(now.frame);
	CHECK(!d.foreground(8, 8));  // 2.5 sigma of a 10-sigma pixel
	CHECK(d.foreground(24, 8));  // 25 levels above a sigma-3 floor
}

TEST(threshold_is_in_units_of_noise)
{
	ChangeDetector detector;
	FrameBuffer scene(32, 32, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 1);
	scene.fillRect(8, 8, 16, 16, yuv(115, 128, 128)); // 15 levels = 5 sigma at the floor
	detector.setAdaptationRate(0.0f);
	detector.setThreshold(5);
	CHECK(!detector.observe(scene.frame).foreground(16, 16));
	detector.setThreshold(4);
	CHECK(detector.observe(scene.frame).foreground(16, 16));
}

TEST(isolated_pixels_are_dropped_and_small_holes_filled)
{
	ChangeDetector detector;
	FrameBuffer scene(32, 32, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 1);
	scene.frame.setLuma(3, 3, 220);                    // lone noisy pixel
	scene.fillRect(12, 12, 12, 12, yuv(220, 128, 128)); // object...
	scene.frame.setLuma(17, 17, 100);                  // ...with a one-pixel hole
	const Detection& d = detector.observe(scene.frame);
	CHECK(!d.foreground(3, 3));
	CHECK(d.foreground(17, 17));
	CHECK(d.foreground(13, 13));
}

TEST(background_follows_slow_changes_away_from_people)
{
	ChangeDetector detector;
	FrameBuffer scene(32, 32, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 1);
	detector.setAdaptationRate(0.2f);
	// One corner slowly brightens by 1 level per frame.
	for (int step = 1; step <= 40; ++step)
	{
		scene.fillRect(0, 0, 8, 8, yuv(100 + step, 128, 128));
		CHECK_EQ(0, countForeground(detector.observe(scene.frame)));
	}
	CHECK(detector.backgroundAt(2, 2).y > 130);
}

TEST(frozen_background_reports_the_same_slow_change)
{
	ChangeDetector detector;
	FrameBuffer scene(32, 32, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 1);
	detector.setAdaptationRate(0.0f);
	scene.fillRect(0, 0, 8, 8, yuv(140, 128, 128));
	CHECK(detector.observe(scene.frame).foreground(2, 2));
	CHECK_EQ(100, detector.backgroundAt(2, 2).y);
}

TEST(person_standing_still_is_not_absorbed)
{
	ChangeDetector detector;
	FrameBuffer scene(64, 64, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 1);
	detector.setAdaptationRate(0.2f);
	scene.fillRect(16, 16, 24, 24, yuv(200, 128, 128));
	for (int i = 0; i < 100; ++i)
		detector.observe(scene.frame);
	CHECK(detector.observe(scene.frame).foreground(28, 28));
	CHECK_EQ(100, detector.backgroundAt(28, 28).y);
}

TEST(frame_of_another_size_is_never_compared)
{
	ChangeDetector detector;
	FrameBuffer small(64, 48, yuv(100, 128, 128));
	learnFrom(detector, small.frame, 1);
	FrameBuffer big(1280, 720, yuv(250, 30, 30));
	const Detection& d = detector.observe(big.frame);
	CHECK(!d.valid());
	CHECK_EQ(160, d.blocks().cols());
	CHECK_EQ(90, d.blocks().rows());
	CHECK(!d.blocks().anyChanged());
}

TEST(works_beyond_640x480)
{
	// Regression: the old arrays were fixed at 640x480 and 60x80 blocks.
	ChangeDetector detector;
	FrameBuffer scene(1280, 720, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 1);
	scene.fillRect(1272, 712, 8, 8, yuv(220, 128, 128));
	const Detection& d = detector.observe(scene.frame);
	CHECK(d.blocks().changed(159, 89));
	CHECK_EQ(1, countBlocks(d.blocks()));
}

TEST(full_hd_is_detected_on_half_size_cells)
{
	ChangeDetector detector;
	FrameBuffer room(1920, 1080, yuv(100, 128, 128));
	learnFrom(detector, room.frame, 1);
	room.fillRect(800, 400, 160, 240, yuv(200, 90, 160)); // a person
	const Detection& d = detector.observe(room.frame);
	CHECK_EQ(2, d.scale());
	CHECK_EQ(1920, d.width());
	CHECK(d.foreground(880, 520));
	// Still reported per frame pixel: the edge lands on the right pixel.
	CHECK(d.foreground(959, 520));
	CHECK(!d.foreground(960, 520));
	CHECK(!d.foreground(200, 200));
	CHECK(d.blocks().changed(110, 65));
	CHECK(!d.blocks().changed(20, 20));
	CHECK_EQ(20 * 30, countBlocks(d.blocks()));
	CHECK_EQ(1920, detector.backgroundWidth());
}

TEST(cell_size_doubles_until_the_width_fits)
{
	FrameBuffer fb(64, 48, yuv(100, 128, 128));
	const int widths[] = { 64, 32, 16, 8, 1 };
	const int scales[] = { 1, 2, 4, 8, 8 }; // never coarser than one block
	for (int i = 0; i < 5; ++i)
	{
		ChangeDetector detector;
		detector.setMaxDetectionWidth(widths[i]);
		learnFrom(detector, fb.frame, 1);
		CHECK_EQ(scales[i], detector.observe(fb.frame).scale());
	}
}

TEST(scaled_detection_marks_the_right_blocks_and_pixels)
{
	ChangeDetector detector;
	detector.setMaxDetectionWidth(16); // 64 wide -> 4x4 pixel cells
	FrameBuffer scene(64, 48, yuv(100, 128, 128));
	learnFrom(detector, scene.frame, 1);
	scene.fillRect(16, 16, 24, 24, yuv(200, 128, 128)); // blocks 2..4
	const Detection& d = detector.observe(scene.frame);
	CHECK_EQ(4, d.scale());
	CHECK(d.foreground(28, 28));
	CHECK(!d.foreground(4, 4));
	CHECK(d.blocks().changed(3, 3));
	CHECK(!d.blocks().changed(0, 0));
	CHECK(!d.blocks().changed(6, 4));
}

TEST(cells_hold_the_average_of_their_pixels)
{
	ChangeDetector detector;
	detector.setMaxDetectionWidth(8); // 16 wide -> 2x2 pixel cells
	FrameBuffer stripes(16, 8, yuv(100, 128, 128));
	for (int y = 0; y < 8; ++y)
		for (int x = 1; x < 16; x += 2)
			stripes.frame.setLuma(x, y, 120);
	learnFrom(detector, stripes.frame, 1);
	CHECK_EQ(110, detector.backgroundAt(0, 0).y);
	CHECK_EQ(110, detector.backgroundAt(15, 7).y);
	// The same stripes are not a change.
	CHECK(!detector.observe(stripes.frame).foreground(5, 5));
}

TEST(changing_the_cell_size_while_learning_waits_for_the_next_run)
{
	ChangeDetector detector;
	FrameBuffer fb(64, 48, yuv(100, 128, 128));
	detector.startLearning(2);
	detector.learn(fb.frame);
	detector.setMaxDetectionWidth(16);
	CHECK(detector.learn(fb.frame));
	CHECK_EQ(1, detector.observe(fb.frame).scale());
	learnFrom(detector, fb.frame, 1);
	CHECK_EQ(4, detector.observe(fb.frame).scale());
}

TEST(scaled_background_mirrors_with_the_frame)
{
	ChangeDetector detector;
	detector.setMaxDetectionWidth(16);
	FrameBuffer scene(64, 32, yuv(100, 128, 128));
	scene.fillRect(0, 0, 16, 32, yuv(200, 128, 128));
	learnFrom(detector, scene.frame, 1);
	scene.frame.mirrorHorizontally();
	detector.mirrorBackground();
	CHECK_EQ(0, countForeground(detector.observe(scene.frame)));
}

TEST(mirrored_background_matches_mirrored_frame)
{
	ChangeDetector detector;
	FrameBuffer scene(32, 16, yuv(100, 128, 128));
	scene.fillRect(0, 0, 8, 16, yuv(200, 128, 128));
	learnFrom(detector, scene.frame, 1);
	scene.frame.mirrorHorizontally();
	detector.mirrorBackground();
	CHECK_EQ(0, countForeground(detector.observe(scene.frame)));
}
