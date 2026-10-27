#ifndef WEBCAM_CORE_EFFECTCHAIN_H
#define WEBCAM_CORE_EFFECTCHAIN_H

#include "ChangeDetector.h"
#include "Yuy2Frame.h"

#include <functional>
#include <memory>
#include <vector>

namespace webcam {

// What an effect may read besides the frame it is drawing on.
struct FrameContext
{
	const ChangeDetector& detector;
	// This frame compared with the background, before any effect ran.
	// Invalid (nothing is foreground) unless some effect needsDetection().
	const Detection& detection;
	// The frame as captured (after mirroring), before any effect ran.
	// Null unless some effect needsOriginal().
	const Yuy2Frame* original;
};

class Effect
{
public:
	// Effects run in ascending stage order; equal stages keep insertion order.
	enum Stage
	{
		kStageAnalysisView = 10, // replaces the picture with detection output
		kStageBackground = 20,   // replaces the scene behind the user
		kStageColor = 30,        // recolors pixels
		kStageShape = 40,        // fixed shapes
		kStageSprite = 50,       // virtual objects
		kStageTrigger = 60       // objects that act when touched
	};

	virtual ~Effect() {}
	virtual int stage() const = 0;
	virtual bool needsDetection() const { return false; }
	virtual bool needsOriginal() const { return false; }
	virtual void apply(Yuy2Frame& frame, const FrameContext& context) = 0;
};

// Runs every frame through: optional mirror, background learning, change
// detection, then the active effects in stage order. Mirroring comes first so
// detection, sprite positions, and what the user sees share one coordinate space.
class EffectChain
{
public:
	EffectChain() : mirror_(false) {}

	ChangeDetector& detector() { return detector_; }
	const ChangeDetector& detector() const { return detector_; }

	// Also flips an already learned background so detection stays aligned.
	void setMirror(bool on);
	bool mirror() const { return mirror_; }

	// onReady runs on the frame that completes the background.
	void startBackgroundLearning(int frameCount, std::function<void()> onReady);

	void add(const std::shared_ptr<Effect>& effect);
	void remove(const std::shared_ptr<Effect>& effect);
	bool contains(const std::shared_ptr<Effect>& effect) const;
	void clear();
	bool empty() const { return effects_.empty(); }

	void process(Yuy2Frame& frame);

private:
	ChangeDetector detector_;
	bool mirror_;
	std::function<void()> onBackgroundReady_;
	std::vector<std::shared_ptr<Effect> > effects_;
	Yuy2Image original_;
	Detection noDetection_;
};

} // namespace webcam

#endif
