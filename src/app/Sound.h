#ifndef WEBCAM_APP_SOUND_H
#define WEBCAM_APP_SOUND_H

#include <memory>

namespace app {

// Short sound effects, synthesized at start-up, so no audio files are needed.
class Sound
{
public:
	enum Effect { kBeep, kGo, kShutter, kPop, kFanfare, kCount };

	Sound();
	~Sound();
	Sound(const Sound&) = delete;
	Sound& operator=(const Sound&) = delete;

	void play(Effect effect);
	void setEnabled(bool enabled) { enabled_ = enabled; }
	bool enabled() const { return enabled_; }

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
	bool enabled_ = true;
};

} // namespace app

#endif
