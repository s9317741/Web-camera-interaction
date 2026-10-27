#ifndef WEBCAM_APP_MUSIC_H
#define WEBCAM_APP_MUSIC_H

#include <filesystem>
#include <memory>
#include <string>

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

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace app

#endif
