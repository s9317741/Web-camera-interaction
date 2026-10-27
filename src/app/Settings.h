#ifndef WEBCAM_APP_SETTINGS_H
#define WEBCAM_APP_SETTINGS_H

#include <filesystem>
#include <map>
#include <string>

namespace app {

// "key = value" lines in a UTF-8 text file. Unknown keys are kept, so older
// and newer versions of the program can share one file.
class Settings
{
public:
	bool load(const std::filesystem::path& file);
	bool save(const std::filesystem::path& file) const;

	int getInt(const std::string& key, int fallback) const;
	float getFloat(const std::string& key, float fallback) const;
	bool getBool(const std::string& key, bool fallback) const;
	std::string getString(const std::string& key, const std::string& fallback) const;

	void set(const std::string& key, int value);
	void set(const std::string& key, float value);
	void set(const std::string& key, bool value);
	void set(const std::string& key, const std::string& value);

private:
	std::map<std::string, std::string> values_;
};

} // namespace app

#endif
