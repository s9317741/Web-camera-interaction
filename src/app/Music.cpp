#include "Music.h"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>

namespace app {

namespace {

std::vector<float> measure(const std::filesystem::path& file, const std::atomic<bool>* cancel)
{
	// Mono at a low rate is plenty for "how loud".
	const ma_uint32 rate = 8000;
	const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, rate);
	ma_decoder decoder;
#if defined(_WIN32)
	if (ma_decoder_init_file_w(file.c_str(), &config, &decoder) != MA_SUCCESS)
#else
	if (ma_decoder_init_file(file.c_str(), &config, &decoder) != MA_SUCCESS)
#endif
		return {};
	const ma_uint64 window = rate / Music::kLevelRate;
	std::vector<float> samples(static_cast<std::size_t>(window));
	std::vector<float> levels;
	while (cancel == nullptr || !*cancel)
	{
		ma_uint64 got = 0;
		ma_decoder_read_pcm_frames(&decoder, samples.data(), window, &got);
		if (got == 0)
			break;
		double sum = 0.0;
		for (ma_uint64 i = 0; i < got; ++i)
			sum += static_cast<double>(samples[i]) * samples[i];
		levels.push_back(static_cast<float>(std::sqrt(sum / static_cast<double>(got))));
	}
	ma_decoder_uninit(&decoder);
	if (levels.empty())
		return levels;
	// The 95th percentile counts as "loud", so a few peaks do not flatten the rest.
	std::vector<float> sorted = levels;
	const std::size_t k = sorted.size() * 95 / 100;
	std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(k), sorted.end());
	const float loud = std::max(sorted[k], 1e-4f);
	for (float& level : levels)
		level = std::min(1.0f, level / loud);
	return levels;
}

} // namespace

struct Music::Impl
{
	ma_engine engine;
	ma_sound sound;
	bool engineReady = false;
	bool soundReady = false;
	float volume = 1.0f;
	bool looping = true;

	std::thread measuring;
	std::atomic<bool> cancel{false};
	mutable std::mutex mutex; // guards levels
	std::vector<float> levels;

	void stopMeasuring()
	{
		cancel = true;
		if (measuring.joinable())
			measuring.join();
		cancel = false;
		std::lock_guard<std::mutex> lock(mutex);
		levels.clear();
	}
};

Music::Music() : impl_(new Impl())
{
	impl_->engineReady = ma_engine_init(nullptr, &impl_->engine) == MA_SUCCESS;
}

Music::~Music()
{
	impl_->stopMeasuring();
	if (impl_->soundReady)
		ma_sound_uninit(&impl_->sound);
	if (impl_->engineReady)
		ma_engine_uninit(&impl_->engine);
}

bool Music::load(const std::filesystem::path& file, std::string& error)
{
	if (!impl_->engineReady)
	{
		error = "No audio output device.";
		return false;
	}
	if (impl_->soundReady)
	{
		ma_sound_uninit(&impl_->sound);
		impl_->soundReady = false;
	}
	const ma_uint32 flags = MA_SOUND_FLAG_STREAM;
#if defined(_WIN32)
	const ma_result result = ma_sound_init_from_file_w(&impl_->engine, file.c_str(), flags,
		nullptr, nullptr, &impl_->sound);
#else
	const ma_result result = ma_sound_init_from_file(&impl_->engine, file.c_str(), flags,
		nullptr, nullptr, &impl_->sound);
#endif
	if (result != MA_SUCCESS)
	{
		error = std::string("Cannot open the file: ") + ma_result_description(result);
		return false;
	}
	impl_->soundReady = true;
	ma_sound_set_volume(&impl_->sound, impl_->volume);
	ma_sound_set_looping(&impl_->sound, impl_->looping ? MA_TRUE : MA_FALSE);

	impl_->stopMeasuring();
	Impl* impl = impl_.get();
	impl_->measuring = std::thread([impl, file] {
		std::vector<float> levels = measure(file, &impl->cancel);
		std::lock_guard<std::mutex> lock(impl->mutex);
		impl->levels = std::move(levels);
	});
	return true;
}

bool Music::loaded() const
{
	return impl_->soundReady;
}

void Music::play()
{
	if (impl_->soundReady)
		ma_sound_start(&impl_->sound);
}

void Music::pause()
{
	if (impl_->soundReady)
		ma_sound_stop(&impl_->sound);
}

void Music::stop()
{
	if (!impl_->soundReady)
		return;
	ma_sound_stop(&impl_->sound);
	ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
}

bool Music::playing() const
{
	return impl_->soundReady && ma_sound_is_playing(&impl_->sound);
}

void Music::setVolume(float volume)
{
	impl_->volume = volume;
	if (impl_->soundReady)
		ma_sound_set_volume(&impl_->sound, volume);
}

void Music::setLooping(bool looping)
{
	impl_->looping = looping;
	if (impl_->soundReady)
		ma_sound_set_looping(&impl_->sound, looping ? MA_TRUE : MA_FALSE);
}

float Music::position() const
{
	float seconds = 0.0f;
	if (impl_->soundReady)
		ma_sound_get_cursor_in_seconds(&impl_->sound, &seconds);
	return seconds;
}

float Music::length() const
{
	float seconds = 0.0f;
	if (impl_->soundReady)
		ma_sound_get_length_in_seconds(&impl_->sound, &seconds);
	return seconds;
}

float Music::level() const
{
	if (!playing())
		return 0.0f;
	const std::size_t at = static_cast<std::size_t>(std::max(0.0f, position()) * kLevelRate);
	std::lock_guard<std::mutex> lock(impl_->mutex);
	return at < impl_->levels.size() ? impl_->levels[at] : 0.0f;
}

std::vector<float> Music::loudness(const std::filesystem::path& file)
{
	return measure(file, nullptr);
}

} // namespace app
