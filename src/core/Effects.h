#ifndef WEBCAM_CORE_EFFECTS_H
#define WEBCAM_CORE_EFFECTS_H

#include "ColorSpace.h"
#include "EffectChain.h"
#include "Sprite.h"

#include <functional>
#include <memory>

namespace webcam {

// Sets U and V of every pixel. Gray is TintEffect(Yuv{y, 128, 128}).
class TintEffect : public Effect
{
public:
	explicit TintEffect(Yuv color) : color_(color) {}
	void setColor(Yuv color) { color_ = color; }
	int stage() const { return kStageColor; }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	Yuv color_;
};

// Tints pixel pairs whose colors both fall inside the range.
class ColorHighlightEffect : public Effect
{
public:
	ColorHighlightEffect(std::shared_ptr<const YuvRange> range, Yuv tint)
		: range_(range), tint_(tint) {}
	int stage() const { return kStageColor; }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	std::shared_ptr<const YuvRange> range_;
	Yuv tint_;
};

// Replaces pixels inside the color range with a picture stretched to the frame.
class ChromaKeyEffect : public Effect
{
public:
	ChromaKeyEffect(std::shared_ptr<const YuvRange> range, std::shared_ptr<const Image> picture)
		: range_(range), picture_(picture) {}
	void setPicture(std::shared_ptr<const Image> picture) { picture_ = picture; }
	int stage() const { return kStageBackground; }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	std::shared_ptr<const YuvRange> range_;
	std::shared_ptr<const Image> picture_;
};

// Shows what the change detector sees instead of the camera picture.
// Leaves the frame alone until a background of the frame's size exists.
class DifferenceViewEffect : public Effect
{
public:
	enum Mode
	{
		kColor,  // per-pixel difference, Y as magnitude and U/V signed
		kBinary, // foreground white, shadow gray, background black
		kBlocks  // changed 8x8 blocks white, others black
	};

	explicit DifferenceViewEffect(Mode mode) : mode_(mode) {}
	void setMode(Mode mode) { mode_ = mode; }
	Mode mode() const { return mode_; }
	int stage() const { return kStageAnalysisView; }
	bool needsDetection() const { return mode_ != kColor; }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	Mode mode_;
};

// Replaces everything that is not foreground (shadows included) with a
// picture stretched to the frame: a virtual background without a green screen.
// Leaves the frame alone until a background of the frame's size exists.
class BackgroundReplaceEffect : public Effect
{
public:
	explicit BackgroundReplaceEffect(std::shared_ptr<const Image> picture) : picture_(picture) {}
	void setPicture(std::shared_ptr<const Image> picture) { picture_ = picture; }
	int stage() const { return kStageBackground; }
	bool needsDetection() const { return true; }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	std::shared_ptr<const Image> picture_;
};

class FillRectEffect : public Effect
{
public:
	FillRectEffect(int x, int y, int width, int height, Yuv color)
		: x_(x), y_(y), width_(width), height_(height), color_(color) {}
	void setColor(Yuv color) { color_ = color; }
	int stage() const { return kStageShape; }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	int x_, y_, width_, height_;
	Yuv color_;
};

class SpriteEffect : public Effect
{
public:
	explicit SpriteEffect(std::shared_ptr<Sprite> sprite) : sprite_(sprite) {}
	int stage() const { return kStageSprite; }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	std::shared_ptr<Sprite> sprite_;
};

// A sprite that disappears once something real moves inside it.
// Optionally paints the change blocks underneath (black and white).
class TouchToHideEffect : public Effect
{
public:
	explicit TouchToHideEffect(std::shared_ptr<Sprite> sprite)
		: sprite_(sprite), showBlocks_(false) {}
	void setShowBlocks(bool on) { showBlocks_ = on; }
	int stage() const { return kStageSprite; }
	bool needsDetection() const { return true; }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	std::shared_ptr<Sprite> sprite_;
	bool showBlocks_;
};

// A sprite that, when touched, hides itself and reports the finished frame
// (without the sprite) together with the frame as captured.
class TouchTriggerEffect : public Effect
{
public:
	typedef std::function<void(const Yuy2Frame& shown, const Yuy2Frame& captured)> Handler;

	TouchTriggerEffect(std::shared_ptr<Sprite> sprite, Handler onTouched)
		: sprite_(sprite), onTouched_(onTouched) {}
	int stage() const { return kStageTrigger; }
	bool needsDetection() const { return sprite_->visible(); }
	bool needsOriginal() const { return sprite_->visible(); }
	void apply(Yuy2Frame& frame, const FrameContext& context);

private:
	std::shared_ptr<Sprite> sprite_;
	Handler onTouched_;
};

// Paints every block white when changed, black otherwise.
void paintBlocks(Yuy2Frame& frame, const BlockMask& changes);

} // namespace webcam

#endif
