#include "Music.h"

#include <miniaudio.h>

namespace app {

struct Music::Impl
{
	ma_engine engine;
	ma_sound sound;
	bool engineReady = false;
	bool soundReady = false;
	float volume = 1.0f;
	bool looping = true;
};

Music::Music() : impl_(new Impl())
{
	impl_->engineReady = ma_engine_init(nullptr, &impl_->engine) == MA_SUCCESS;
}

Music::~Music()
{
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

} // namespace app
