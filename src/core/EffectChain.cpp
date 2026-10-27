#include "EffectChain.h"

#include <algorithm>

namespace webcam {

void EffectChain::setMirror(bool on)
{
	if (on != mirror_ && detector_.hasBackground())
		detector_.mirrorBackground();
	mirror_ = on;
}

void EffectChain::startBackgroundLearning(int frameCount, std::function<void()> onReady)
{
	onBackgroundReady_ = onReady;
	detector_.startLearning(frameCount);
}

void EffectChain::add(const std::shared_ptr<Effect>& effect)
{
	if (!effect || contains(effect))
		return;
	std::vector<std::shared_ptr<Effect> >::iterator at = effects_.begin();
	while (at != effects_.end() && (*at)->stage() <= effect->stage())
		++at;
	effects_.insert(at, effect);
}

void EffectChain::remove(const std::shared_ptr<Effect>& effect)
{
	effects_.erase(std::remove(effects_.begin(), effects_.end(), effect), effects_.end());
}

bool EffectChain::contains(const std::shared_ptr<Effect>& effect) const
{
	return std::find(effects_.begin(), effects_.end(), effect) != effects_.end();
}

void EffectChain::clear()
{
	effects_.clear();
}

void EffectChain::process(Yuy2Frame& frame)
{
	if (frame.empty())
		return;

	if (mirror_)
		frame.mirrorHorizontally();

	if (detector_.isLearning() && detector_.learn(frame) && onBackgroundReady_)
	{
		std::function<void()> ready = onBackgroundReady_;
		onBackgroundReady_ = std::function<void()>();
		ready();
	}

	// Effects may add or remove effects from callbacks, so run a snapshot.
	const std::vector<std::shared_ptr<Effect> > active = effects_;

	bool wantDetection = false;
	bool wantOriginal = false;
	for (std::size_t i = 0; i < active.size(); ++i)
	{
		wantDetection = wantDetection || active[i]->needsDetection();
		wantOriginal = wantOriginal || active[i]->needsOriginal();
	}

	const bool useExternal = external_.valid() && external_.width() == frame.width() &&
		external_.height() == frame.height();
	const Detection& detection = !wantDetection ? noDetection_
		: (useExternal ? external_ : detector_.observe(frame));
	if (wantOriginal)
		original_.assign(frame);

	const FrameContext context = { detector_, detection, wantOriginal ? &original_.frame() : 0 };
	for (std::size_t i = 0; i < active.size(); ++i)
		active[i]->apply(frame, context);
}

} // namespace webcam
