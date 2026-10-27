#include "Sound.h"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace app {

namespace {

const ma_uint32 kRate = 44100;
const double kPi = 3.14159265358979323846;

// A tone with a quick attack and an exponential fade.
void addTone(std::vector<float>& out, double start, double seconds, double frequency, double volume, double decay)
{
	const std::size_t first = static_cast<std::size_t>(start * kRate);
	const std::size_t count = static_cast<std::size_t>(seconds * kRate);
	if (out.size() < first + count)
		out.resize(first + count, 0.0f);
	for (std::size_t i = 0; i < count; ++i)
	{
		const double t = static_cast<double>(i) / kRate;
		const double envelope = std::min(1.0, t * 400.0) * std::exp(-t * decay);
		out[first + i] += static_cast<float>(volume * envelope * std::sin(2.0 * kPi * frequency * t));
	}
}

std::vector<float> synthesize(Sound::Effect effect)
{
	std::vector<float> s;
	switch (effect)
	{
	case Sound::kBeep: // countdown tick
		addTone(s, 0.0, 0.12, 880.0, 0.35, 18.0);
		break;
	case Sound::kGo: // last tick, higher
		addTone(s, 0.0, 0.25, 1320.0, 0.35, 10.0);
		break;
	case Sound::kShutter: // two clicks of filtered noise
	{
		s.resize(static_cast<std::size_t>(0.16 * kRate), 0.0f);
		std::srand(7);
		float smooth = 0.0f;
		for (std::size_t i = 0; i < s.size(); ++i)
		{
			const double t = static_cast<double>(i) / kRate;
			const double click = std::exp(-t * 90.0) + (t > 0.07 ? std::exp(-(t - 0.07) * 70.0) * 0.8 : 0.0);
			const float noise = static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f;
			smooth = smooth * 0.55f + noise * 0.45f;
			s[i] = static_cast<float>(0.6 * click * smooth);
		}
		break;
	}
	case Sound::kPop: // a catch in the game
		for (int i = 0; i < 6; ++i)
			addTone(s, i * 0.012, 0.08, 500.0 + i * 140.0, 0.22, 30.0);
		break;
	case Sound::kFanfare: // time is up
		addTone(s, 0.00, 0.20, 523.25, 0.25, 6.0);
		addTone(s, 0.15, 0.20, 659.25, 0.25, 6.0);
		addTone(s, 0.30, 0.45, 783.99, 0.30, 4.0);
		break;
	default:
		break;
	}
	return s;
}

} // namespace

struct Sound::Impl
{
	ma_engine engine;
	bool engineReady = false;
	ma_audio_buffer buffers[kCount];
	ma_sound sounds[kCount];
	bool ready[kCount] = {};
};

Sound::Sound() : impl_(new Impl())
{
	impl_->engineReady = ma_engine_init(nullptr, &impl_->engine) == MA_SUCCESS;
	if (!impl_->engineReady)
		return;
	for (int i = 0; i < kCount; ++i)
	{
		const std::vector<float> samples = synthesize(static_cast<Effect>(i));
		ma_audio_buffer_config config = ma_audio_buffer_config_init(ma_format_f32, 1, samples.size(), samples.data(), nullptr);
		config.sampleRate = kRate;
		if (ma_audio_buffer_init_copy(&config, &impl_->buffers[i]) != MA_SUCCESS)
			continue;
		if (ma_sound_init_from_data_source(&impl_->engine, &impl_->buffers[i], MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr,
			&impl_->sounds[i]) != MA_SUCCESS)
		{
			ma_audio_buffer_uninit(&impl_->buffers[i]);
			continue;
		}
		impl_->ready[i] = true;
	}
}

Sound::~Sound()
{
	for (int i = 0; i < kCount; ++i)
		if (impl_->ready[i])
		{
			ma_sound_uninit(&impl_->sounds[i]);
			ma_audio_buffer_uninit(&impl_->buffers[i]);
		}
	if (impl_->engineReady)
		ma_engine_uninit(&impl_->engine);
}

void Sound::play(Effect effect)
{
	if (!enabled_ || effect < 0 || effect >= kCount || !impl_->ready[effect])
		return;
	ma_sound* sound = &impl_->sounds[effect];
	ma_sound_stop(sound);
	ma_sound_seek_to_pcm_frame(sound, 0);
	ma_sound_start(sound);
}

} // namespace app
