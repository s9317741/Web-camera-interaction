#ifndef WEBCAM_APP_MUSIC_H
#define WEBCAM_APP_MUSIC_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace app {

// Plays one audio file (MP3, WAV, FLAC) through the default output device.
class Music
{
public:
	Music();
	~Music();
	Music(const Music&) = delete;
	Music& operator=(const Music&) = delete;

	// Replaces the current track. On failure the reason is in `error`.
	bool load(const std::filesystem::path& file, std::string& error);
	bool loaded() const;

	void play();
	void pause();
	void stop(); // pauses and rewinds
	bool playing() const;

	void setVolume(float volume); // 0..1
	void setLooping(bool looping);

	// Position and length in seconds; 0 when unknown.
	float position() const;
	float length() const;

	// How loud the music is at this moment, 0..1; 0 while stopped, or while
	// a newly loaded track is still being measured (on a thread of its own).
	float level() const;

	// The loudness of a whole file, kLevelRate values a second, scaled so
	// the loud passages are near 1. Empty when the file cannot be read.
	static const int kLevelRate = 50;
	static std::vector<float> loudness(const std::filesystem::path& file);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace app

#endif
