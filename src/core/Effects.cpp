#include "Effects.h"

#include <algorithm>
#include <cstdlib>

namespace webcam {

namespace {

const std::uint8_t kBlack = 16;
const std::uint8_t kWhite = 255;
const std::uint8_t kNeutral = 128;

void setGray(Yuy2Frame& frame, int x, int y, std::uint8_t luma)
{
	frame.setLuma(x, y, luma);
	frame.setChroma(x, y, kNeutral, kNeutral);
}

// A picture stretched over the frame, with the source column and row of every
// frame column and row worked out once per frame instead of per pixel.
// Picks the same pixels as Image::sampleStretched.
class StretchedPicture
{
public:
	StretchedPicture(const Image& picture, int width, int height) : picture_(picture)
	{
		map(width, picture.width(), columns_);
		map(height, picture.height(), rows_);
	}

	Yuv at(int x, int y) const { return picture_.at(columns_[x], rows_[y]); }

private:
	static void map(int target, int source, std::vector<int>& out)
	{
		out.resize(target);
		for (int i = 0; i < target; ++i)
			out[i] = std::min(static_cast<int>(static_cast<long long>(i) * source / target), source - 1);
	}

	const Image& picture_;
	std::vector<int> columns_;
	std::vector<int> rows_;
};

} // namespace

void paintBlocks(Yuy2Frame& frame, const BlockMask& changes)
{
	const int size = changes.blockSize() > 0 ? changes.blockSize() : ChangeDetector::kBlockSize;
	for (int y = 0; y < frame.height(); ++y)
		for (int x = 0; x < frame.width(); ++x)
			setGray(frame, x, y, changes.changed(x / size, y / size) ? kWhite : kBlack);
}

void TintEffect::apply(Yuy2Frame& frame, const FrameContext&)
{
	for (int y = 0; y < frame.height(); ++y)
		for (int x = 0; x < frame.width(); x += 2)
			frame.setChroma(x, y, color_.u, color_.v);
}

void ColorHighlightEffect::apply(Yuy2Frame& frame, const FrameContext&)
{
	for (int y = 0; y < frame.height(); ++y)
		for (int x = 0; x < frame.width(); x += 2)
			if (range_->contains(frame.pixel(x, y)) && range_->contains(frame.pixel(x + 1, y)))
				frame.setChroma(x, y, tint_.u, tint_.v);
}

void ChromaKeyEffect::apply(Yuy2Frame& frame, const FrameContext&)
{
	if (!picture_ || picture_->width() == 0)
		return;
	const int w = frame.width();
	const int h = frame.height();
	const StretchedPicture picture(*picture_, w, h);
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; x += 2)
		{
			// Decide for both pixels before writing: they share U and V.
			const bool key0 = range_->contains(frame.pixel(x, y));
			const bool key1 = range_->contains(frame.pixel(x + 1, y));
			if (!key0 && !key1)
				continue;
			const Yuv p0 = picture.at(x, y);
			const Yuv p1 = picture.at(x + 1, y);
			if (key0)
				frame.setLuma(x, y, p0.y);
			if (key1)
				frame.setLuma(x + 1, y, p1.y);
			const Yuv chroma = key0 ? p0 : p1;
			frame.setChroma(x, y, chroma.u, chroma.v);
		}
}

void DifferenceViewEffect::apply(Yuy2Frame& frame, const FrameContext& context)
{
	const ChangeDetector& detector = context.detector;
	if (!detector.hasBackgroundFor(frame))
		return;

	if (mode_ == kBlocks)
	{
		paintBlocks(frame, context.detection.blocks());
		return;
	}

	if (mode_ == kBinary)
	{
		const Detection& d = context.detection;
		for (int y = 0; y < frame.height(); ++y)
			for (int x = 0; x < frame.width(); ++x)
				setGray(frame, x, y, d.foreground(x, y) ? kWhite : (d.shadow(x, y) ? kNeutral : kBlack));
		return;
	}

	for (int y = 0; y < frame.height(); ++y)
		for (int x = 0; x < frame.width(); x += 2)
		{
			// Read both pixels before writing: they share U and V.
			const Yuv now0 = frame.pixel(x, y);
			const Yuv now1 = frame.pixel(x + 1, y);
			const Yuv bg0 = detector.backgroundAt(x, y);
			const Yuv bg1 = detector.backgroundAt(x + 1, y);
			frame.setLuma(x, y, clampToByte(std::abs(now0.y - bg0.y) + kBlack));
			frame.setLuma(x + 1, y, clampToByte(std::abs(now1.y - bg1.y) + kBlack));
			frame.setChroma(x, y, clampToByte(bg0.u - now0.u + kNeutral),
				clampToByte(bg0.v - now0.v + kNeutral));
		}
}

void BackgroundReplaceEffect::apply(Yuy2Frame& frame, const FrameContext& context)
{
	const Detection& d = context.detection;
	if (!d.valid() || !picture_ || picture_->width() == 0)
		return;
	const int w = frame.width();
	const int h = frame.height();
	const StretchedPicture picture(*picture_, w, h);
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; x += 2)
		{
			const bool keep0 = d.foreground(x, y);
			const bool keep1 = d.foreground(x + 1, y);
			if (keep0 && keep1)
				continue;
			const Yuv p0 = picture.at(x, y);
			const Yuv p1 = picture.at(x + 1, y);
			if (!keep0)
				frame.setLuma(x, y, p0.y);
			if (!keep1)
				frame.setLuma(x + 1, y, p1.y);
			// A pair split by the edge keeps the person's color.
			if (!keep0 && !keep1)
				frame.setChroma(x, y, p0.u, p0.v);
		}
}

void FillRectEffect::apply(Yuy2Frame& frame, const FrameContext&)
{
	const int x0 = std::max(0, x_);
	const int y0 = std::max(0, y_);
	const int x1 = std::min(frame.width(), x_ + width_);
	const int y1 = std::min(frame.height(), y_ + height_);
	for (int y = y0; y < y1; ++y)
		for (int x = x0; x < x1; ++x)
			frame.setPixel(x, y, color_);
}

void SpriteEffect::apply(Yuy2Frame& frame, const FrameContext&)
{
	sprite_->drawOnto(frame);
}

void TouchToHideEffect::apply(Yuy2Frame& frame, const FrameContext& context)
{
	if (showBlocks_)
		paintBlocks(frame, context.detection.blocks());
	if (sprite_->visible() && sprite_->overlaps(context.detection.blocks()))
		sprite_->hide();
	sprite_->drawOnto(frame);
}

void TouchTriggerEffect::apply(Yuy2Frame& frame, const FrameContext& context)
{
	const bool touched = sprite_->overlaps(context.detection.blocks());
	if (!sprite_->visible())
	{
		if (rearmFrames_ <= 0)
			return;
		quietFrames_ = touched ? 0 : quietFrames_ + 1;
		if (quietFrames_ < rearmFrames_)
			return;
		quietFrames_ = 0;
		sprite_->show();
		sprite_->drawOnto(frame);
		return;
	}
	if (touched)
	{
		sprite_->hide();
		quietFrames_ = 0;
		if (onTouched_)
			onTouched_(frame, context.original ? *context.original : frame);
		return;
	}
	sprite_->drawOnto(frame);
}

} // namespace webcam
