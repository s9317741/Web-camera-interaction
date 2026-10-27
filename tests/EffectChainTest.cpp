#include "Check.h"
#include "FrameBuffer.h"

#include "core/EffectChain.h"
#include "core/Effects.h"

#include <memory>
#include <string>

using namespace webcam;

namespace {

// Records the order effects run in.
class Recorder : public Effect
{
public:
	Recorder(int stage, char tag, std::string& log) : stage_(stage), tag_(tag), log_(log) {}
	int stage() const { return stage_; }
	void apply(Yuy2Frame&, const FrameContext&) { log_ += tag_; }

private:
	int stage_;
	char tag_;
	std::string& log_;
};

std::shared_ptr<Sprite> whiteSquare(int size, int x, int y)
{
	BgrPicture pic(size, size, rgb(255, 255, 255));
	return std::make_shared<Sprite>(Image::fromBgr24(&pic.bytes[0], size, size, pic.stride()), x, y);
}

void learnBackground(EffectChain& chain, Yuy2Frame& frame)
{
	chain.startBackgroundLearning(1, std::function<void()>());
	chain.process(frame);
}

} // namespace

TEST(effects_run_in_stage_order_whatever_the_insertion_order)
{
	std::string log;
	EffectChain chain;
	chain.add(std::make_shared<Recorder>(Effect::kStageSprite, 's', log));
	chain.add(std::make_shared<Recorder>(Effect::kStageColor, 'c', log));
	chain.add(std::make_shared<Recorder>(Effect::kStageColor, 'd', log));
	chain.add(std::make_shared<Recorder>(Effect::kStageAnalysisView, 'a', log));
	FrameBuffer fb(4, 2, yuv(16, 128, 128));
	chain.process(fb.frame);
	CHECK(log == "acds");
}

TEST(adding_twice_runs_once_and_remove_stops_it)
{
	std::string log;
	EffectChain chain;
	std::shared_ptr<Effect> e = std::make_shared<Recorder>(Effect::kStageColor, 'x', log);
	chain.add(e);
	chain.add(e);
	FrameBuffer fb(4, 2, yuv(16, 128, 128));
	chain.process(fb.frame);
	chain.remove(e);
	chain.process(fb.frame);
	CHECK(log == "x");
	CHECK(!chain.contains(e));
}

TEST(sprite_lands_where_placed_when_mirrored)
{
	// Regression: the photo mode drew its icon first and mirrored afterwards.
	EffectChain chain;
	chain.setMirror(true);
	chain.add(std::make_shared<SpriteEffect>(whiteSquare(2, 0, 0)));
	FrameBuffer fb(8, 2, yuv(16, 128, 128));
	chain.process(fb.frame);
	CHECK_EQ(235, fb.frame.pixel(0, 0).y);
	CHECK_EQ(16, fb.frame.pixel(7, 0).y);
}

TEST(background_ready_callback_fires_once)
{
	EffectChain chain;
	int ready = 0;
	chain.startBackgroundLearning(2, [&ready]() { ++ready; });
	FrameBuffer fb(8, 8, yuv(100, 128, 128));
	for (int i = 0; i < 4; ++i)
		chain.process(fb.frame);
	CHECK_EQ(1, ready);
	CHECK(chain.detector().hasBackgroundFor(fb.frame));
}

TEST(toggling_mirror_keeps_detection_aligned)
{
	EffectChain chain;
	FrameBuffer scene(16, 8, yuv(100, 128, 128));
	scene.fillRect(0, 0, 8, 8, yuv(200, 128, 128));
	learnBackground(chain, scene.frame);

	chain.setMirror(true);
	FrameBuffer again(16, 8, yuv(100, 128, 128));
	again.fillRect(0, 0, 8, 8, yuv(200, 128, 128));
	chain.process(again.frame); // mirrored inside the chain
	CHECK(!chain.detector().observe(again.frame).blocks().anyChanged());
}

TEST(touch_to_hide_disappears_when_touched)
{
	EffectChain chain;
	FrameBuffer scene(64, 64, yuv(100, 128, 128));
	learnBackground(chain, scene.frame);

	std::shared_ptr<Sprite> gecko = whiteSquare(32, 8, 8);
	chain.add(std::make_shared<TouchToHideEffect>(gecko));

	FrameBuffer quiet(64, 64, yuv(100, 128, 128));
	chain.process(quiet.frame);
	CHECK(gecko->visible());
	CHECK_EQ(235, quiet.frame.pixel(20, 20).y);

	FrameBuffer hand(64, 64, yuv(100, 128, 128));
	hand.fillRect(16, 16, 8, 8, yuv(30, 128, 128)); // block (2, 2), inside the gecko
	chain.process(hand.frame);
	CHECK(!gecko->visible());
	CHECK_EQ(100, hand.frame.pixel(30, 30).y); // inside the gecko, not the hand
}

TEST(touch_to_hide_can_show_blocks_underneath)
{
	// Regression: the block-display checkbox was read by an unused callback.
	EffectChain chain;
	FrameBuffer scene(32, 32, yuv(100, 128, 128));
	learnBackground(chain, scene.frame);
	std::shared_ptr<TouchToHideEffect> game =
		std::make_shared<TouchToHideEffect>(whiteSquare(2, 30, 30));
	game->setShowBlocks(true);
	chain.add(game);

	FrameBuffer hand(32, 32, yuv(100, 128, 128));
	hand.fillRect(0, 0, 8, 8, yuv(30, 128, 128));
	chain.process(hand.frame);
	CHECK_EQ(255, hand.frame.pixel(3, 3).y);
	CHECK_EQ(16, hand.frame.pixel(12, 12).y);
}

TEST(touch_trigger_reports_finished_and_captured_frames_once)
{
	EffectChain chain;
	FrameBuffer scene(64, 64, yuv(100, 128, 128));
	learnBackground(chain, scene.frame);

	YuvRange everything = { 0, 255, 0, 255, 0, 255 };
	BgrPicture beach(4, 4, rgb(0, 0, 255));
	chain.add(std::make_shared<ChromaKeyEffect>(std::make_shared<YuvRange>(everything),
		Image::fromBgr24(&beach.bytes[0], 4, 4, beach.stride())));

	int shots = 0;
	int shownY = -1, capturedY = -1;
	std::shared_ptr<Sprite> button = whiteSquare(32, 8, 8);
	chain.add(std::make_shared<TouchTriggerEffect>(button,
		[&](const Yuy2Frame& shown, const Yuy2Frame& captured) {
			++shots;
			shownY = shown.pixel(0, 0).y;
			capturedY = captured.pixel(0, 0).y;
		}));

	FrameBuffer hand(64, 64, yuv(100, 128, 128));
	hand.fillRect(16, 16, 8, 8, yuv(30, 128, 128));
	chain.process(hand.frame);
	chain.process(hand.frame);

	CHECK_EQ(1, shots);
	CHECK(!button->visible());
	CHECK_EQ(rgbToYuv(rgb(0, 0, 255)).y, shownY); // background replaced
	CHECK_EQ(100, capturedY);                     // as the camera saw it
}

namespace {

// The TakePicture setup from MainForm at 1080p: mirrored, background replaced
// by difference, camera button in the bottom-right corner.
struct PhotoBooth
{
	static const int kW = 1920;
	static const int kH = 1080;

	EffectChain chain;
	std::shared_ptr<Sprite> button;
	int shots;

	PhotoBooth() : shots(0)
	{
		chain.setMirror(true);
		FrameBuffer room(kW, kH, yuv(150, 120, 135));
		chain.startBackgroundLearning(1, std::function<void()>());
		chain.process(room.frame);

		BgrPicture beach(64, 40, rgb(0, 0, 255));
		chain.add(std::make_shared<BackgroundReplaceEffect>(
			Image::fromBgr24(&beach.bytes[0], 64, 40, beach.stride())));
		button = whiteSquare(125, kW - 125 - 15, kH - 125 - 5);
		chain.add(std::make_shared<TouchTriggerEffect>(button,
			[this](const Yuy2Frame&, const Yuy2Frame&) { ++shots; }));
	}

	// Fills a rectangle given in on-screen (mirrored) coordinates; the camera
	// sees it on the other side.
	static void put(FrameBuffer& camera, int x, int y, int w, int h, Yuv c)
	{
		camera.fillRect(kW - x - w, y, w, h, c);
	}
};

const Yuv kSkin = { 170, 105, 150 };

} // namespace

TEST(touching_the_camera_button_takes_one_picture)
{
	PhotoBooth booth;
	FrameBuffer camera(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
	PhotoBooth::put(camera, 760, 300, 400, 780, kSkin);   // the person
	PhotoBooth::put(camera, 1800, 980, 60, 60, kSkin);    // their hand on the button
	booth.chain.process(camera.frame);
	CHECK_EQ(1, booth.shots);
	CHECK(!booth.button->visible());

	FrameBuffer again(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
	PhotoBooth::put(again, 1800, 980, 60, 60, kSkin);
	booth.chain.process(again.frame);
	CHECK_EQ(1, booth.shots);
}

TEST(camera_button_returns_only_after_the_hand_leaves)
{
	PhotoBooth booth;
	std::shared_ptr<TouchTriggerEffect> trigger = std::make_shared<TouchTriggerEffect>(booth.button,
		[&booth](const Yuy2Frame&, const Yuy2Frame&) { ++booth.shots; });
	booth.chain.clear();
	booth.chain.add(trigger);
	trigger->setRearmAfterQuietFrames(5);

	for (int i = 0; i < 20; ++i) // hand stays on the button
	{
		FrameBuffer camera(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
		PhotoBooth::put(camera, 1800, 980, 60, 60, kSkin);
		booth.chain.process(camera.frame);
	}
	CHECK_EQ(1, booth.shots);
	CHECK(!booth.button->visible());

	for (int i = 0; i < 4; ++i) // hand gone, not long enough yet
	{
		FrameBuffer camera(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
		booth.chain.process(camera.frame);
	}
	CHECK(!booth.button->visible());
	FrameBuffer quiet(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
	booth.chain.process(quiet.frame);
	CHECK(booth.button->visible());

	FrameBuffer touch(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
	PhotoBooth::put(touch, 1800, 980, 60, 60, kSkin);
	booth.chain.process(touch.frame);
	CHECK_EQ(2, booth.shots);
}

TEST(standing_in_view_without_touching_takes_no_picture)
{
	PhotoBooth booth;
	for (int i = 0; i < 60; ++i) // two seconds, while the background adapts
	{
		FrameBuffer camera(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
		PhotoBooth::put(camera, 760, 300, 400, 780, kSkin);
		PhotoBooth::put(camera, 1700, 700, 60, 60, kSkin); // hand near, not on, the button
		booth.chain.process(camera.frame);
	}
	CHECK_EQ(0, booth.shots);
	CHECK(booth.button->visible());
}

TEST(hand_on_the_unmirrored_side_takes_no_picture)
{
	// The button is bottom-right on screen; with the mirror on, the camera
	// sees a real hand there at the bottom-left of its own picture.
	PhotoBooth booth;
	FrameBuffer camera(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
	camera.fillRect(1800, 980, 60, 60, kSkin); // bottom-right of the raw camera image
	booth.chain.process(camera.frame);
	CHECK_EQ(0, booth.shots);
}

TEST(shadow_over_the_button_takes_no_picture)
{
	PhotoBooth booth;
	FrameBuffer camera(PhotoBooth::kW, PhotoBooth::kH, yuv(150, 120, 135));
	PhotoBooth::put(camera, 1760, 940, 140, 130, yuv(16 + 134 * 6 / 10, 120, 135));
	booth.chain.process(camera.frame);
	CHECK_EQ(0, booth.shots);
}

TEST(mask_detection_marks_blocks_where_the_mask_is_set)
{
	std::vector<std::uint8_t> mask(32 * 16, 0);
	for (int y = 0; y < 8; ++y)
		for (int x = 8; x < 16; ++x)
			mask[y * 32 + x] = 255; // block (1, 0)
	mask[15 * 32 + 30] = 200;       // a lone pixel is not a block
	const Detection d = Detection::fromMask(&mask[0], 32, 16, 32);
	CHECK(d.valid());
	CHECK(d.foreground(9, 3));
	CHECK(d.foreground(30, 15));
	CHECK(!d.foreground(0, 0));
	CHECK(d.blocks().changed(1, 0));
	CHECK(!d.blocks().changed(3, 1));
}

TEST(external_mask_replaces_the_background_comparison)
{
	// No background learned at all: a mask from elsewhere drives the trigger.
	EffectChain chain;
	int shots = 0;
	std::shared_ptr<Sprite> button = whiteSquare(32, 8, 8);
	chain.add(std::make_shared<TouchTriggerEffect>(button,
		[&shots](const Yuy2Frame&, const Yuy2Frame&) { ++shots; }));

	FrameBuffer fb(64, 64, yuv(100, 128, 128));
	std::vector<std::uint8_t> mask(64 * 64, 0);
	chain.setExternalDetection(Detection::fromMask(&mask[0], 64, 64, 64));
	chain.process(fb.frame);
	CHECK_EQ(0, shots);

	for (int y = 16; y < 24; ++y)
		for (int x = 16; x < 24; ++x)
			mask[y * 64 + x] = 255; // a hand inside the button
	chain.setExternalDetection(Detection::fromMask(&mask[0], 64, 64, 64));
	chain.process(fb.frame);
	CHECK_EQ(1, shots);
}

TEST(external_mask_of_another_size_is_ignored)
{
	EffectChain chain;
	int shots = 0;
	chain.add(std::make_shared<TouchTriggerEffect>(whiteSquare(32, 8, 8),
		[&shots](const Yuy2Frame&, const Yuy2Frame&) { ++shots; }));
	std::vector<std::uint8_t> mask(32 * 32, 255);
	chain.setExternalDetection(Detection::fromMask(&mask[0], 32, 32, 32));
	FrameBuffer fb(64, 64, yuv(100, 128, 128));
	chain.process(fb.frame);
	CHECK_EQ(0, shots);
}

TEST(difference_view_waits_for_a_background)
{
	EffectChain chain;
	chain.add(std::make_shared<DifferenceViewEffect>(DifferenceViewEffect::kBinary));
	FrameBuffer fb(8, 8, yuv(90, 100, 110));
	chain.process(fb.frame);
	CHECK_EQ(90, fb.frame.pixel(0, 0).y);
}

TEST(binary_view_shows_foreground_white_and_shadow_gray)
{
	EffectChain chain;
	FrameBuffer scene(32, 32, yuv(160, 120, 135));
	learnBackground(chain, scene.frame);
	chain.add(std::make_shared<DifferenceViewEffect>(DifferenceViewEffect::kBinary));

	FrameBuffer moved(32, 32, yuv(160, 120, 135));
	moved.fillRect(4, 4, 8, 8, yuv(240, 120, 135));                // object
	moved.fillRect(20, 4, 8, 8, yuv(16 + 144 * 6 / 10, 120, 135)); // shadow
	chain.process(moved.frame);
	CHECK_EQ(255, moved.frame.pixel(8, 8).y);
	CHECK_EQ(128, moved.frame.pixel(24, 8).y);
	CHECK_EQ(16, moved.frame.pixel(8, 24).y);
	CHECK_EQ(128, moved.frame.pixel(8, 8).u);
}

TEST(background_replace_keeps_only_the_person)
{
	EffectChain chain;
	FrameBuffer room(32, 32, yuv(160, 120, 135));
	learnBackground(chain, room.frame);
	BgrPicture beach(4, 4, rgb(0, 0, 255));
	chain.add(std::make_shared<BackgroundReplaceEffect>(
		Image::fromBgr24(&beach.bytes[0], 4, 4, beach.stride())));

	FrameBuffer person(32, 32, yuv(160, 120, 135));
	person.fillRect(8, 8, 12, 12, yuv(220, 90, 170));
	person.fillRect(24, 0, 8, 32, yuv(16 + 144 * 6 / 10, 120, 135)); // their shadow
	chain.process(person.frame);

	const Yuv blue = rgbToYuv(rgb(0, 0, 255));
	CHECK_EQ(220, person.frame.pixel(14, 14).y);
	CHECK_EQ(90, person.frame.pixel(14, 14).u);
	CHECK_EQ(blue.y, person.frame.pixel(2, 2).y);
	CHECK_EQ(blue.u, person.frame.pixel(2, 2).u);
	CHECK_EQ(blue.y, person.frame.pixel(28, 20).y); // the shadow goes too
}

TEST(background_replace_works_on_scaled_detection)
{
	EffectChain chain;
	chain.detector().setMaxDetectionWidth(16); // 64 wide -> 4x4 pixel cells
	FrameBuffer room(64, 48, yuv(160, 120, 135));
	learnBackground(chain, room.frame);
	BgrPicture beach(4, 4, rgb(0, 0, 255));
	chain.add(std::make_shared<BackgroundReplaceEffect>(
		Image::fromBgr24(&beach.bytes[0], 4, 4, beach.stride())));

	FrameBuffer person(64, 48, yuv(160, 120, 135));
	person.fillRect(16, 8, 24, 32, yuv(220, 90, 170));
	chain.process(person.frame);
	CHECK_EQ(220, person.frame.pixel(27, 23).y);
	CHECK_EQ(rgbToYuv(rgb(0, 0, 255)).y, person.frame.pixel(52, 23).y);
}

TEST(background_replace_waits_for_a_background)
{
	EffectChain chain;
	BgrPicture beach(4, 4, rgb(0, 0, 255));
	chain.add(std::make_shared<BackgroundReplaceEffect>(
		Image::fromBgr24(&beach.bytes[0], 4, 4, beach.stride())));
	FrameBuffer fb(16, 16, yuv(90, 100, 110));
	chain.process(fb.frame);
	CHECK_EQ(90, fb.frame.pixel(3, 3).y);
}

TEST(color_highlight_tints_only_matching_pairs)
{
	EffectChain chain;
	YuvRange skin = { 50, 150, 100, 140, 100, 140 };
	chain.add(std::make_shared<ColorHighlightEffect>(std::make_shared<YuvRange>(skin),
		yuv(0, 54, 34)));
	FrameBuffer fb(4, 1, yuv(100, 120, 120));
	fb.frame.setLuma(3, 0, 200); // second pair no longer matches
	chain.process(fb.frame);
	CHECK_EQ(54, fb.frame.pixel(0, 0).u);
	CHECK_EQ(120, fb.frame.pixel(2, 0).u);
}

TEST(chroma_key_replaces_only_keyed_pixels)
{
	EffectChain chain;
	YuvRange green = { 0, 255, 0, 100, 0, 100 };
	BgrPicture beach(2, 1, rgb(255, 255, 255));
	chain.add(std::make_shared<ChromaKeyEffect>(std::make_shared<YuvRange>(green),
		Image::fromBgr24(&beach.bytes[0], 2, 1, beach.stride())));
	FrameBuffer fb(4, 1, yuv(60, 60, 60)); // "green screen"
	fb.frame.setPixel(2, 0, yuv(60, 128, 128));
	fb.frame.setPixel(3, 0, yuv(70, 128, 128));
	chain.process(fb.frame);
	CHECK_EQ(235, fb.frame.pixel(0, 0).y);
	CHECK_EQ(60, fb.frame.pixel(2, 0).y);
	CHECK_EQ(70, fb.frame.pixel(3, 0).y);
}
