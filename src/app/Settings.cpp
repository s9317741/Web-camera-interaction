#include "Settings.h"

#include "Platform.h"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace app {

namespace {

std::string trim(const std::string& text)
{
	const std::size_t first = text.find_first_not_of(" \t\r");
	if (first == std::string::npos)
		return std::string();
	const std::size_t last = text.find_last_not_of(" \t\r");
	return text.substr(first, last - first + 1);
}

} // namespace

bool Settings::load(const std::filesystem::path& file)
{
	std::vector<unsigned char> bytes;
	if (!readFile(file, bytes))
		return false;
	std::istringstream in(std::string(bytes.begin(), bytes.end()));
	std::string line;
	while (std::getline(in, line))
	{
		const std::size_t equals = line.find('=');
		if (line.empty() || line[0] == '#' || equals == std::string::npos)
			continue;
		values_[trim(line.substr(0, equals))] = trim(line.substr(equals + 1));
	}
	return true;
}

bool Settings::save(const std::filesystem::path& file) const
{
	std::string text = "# Webcam Interaction settings, written on exit.\n";
	for (const auto& entry : values_)
		text += entry.first + " = " + entry.second + "\n";
	return writeFile(file, std::vector<unsigned char>(text.begin(), text.end()));
}

int Settings::getInt(const std::string& key, int fallback) const
{
	const auto it = values_.find(key);
	if (it == values_.end())
		return fallback;
	char* end = nullptr;
	const long value = std::strtol(it->second.c_str(), &end, 10);
	return end == it->second.c_str() ? fallback : static_cast<int>(value);
}

float Settings::getFloat(const std::string& key, float fallback) const
{
	const auto it = values_.find(key);
	if (it == values_.end())
		return fallback;
	char* end = nullptr;
	const float value = std::strtof(it->second.c_str(), &end);
	return end == it->second.c_str() ? fallback : value;
}

bool Settings::getBool(const std::string& key, bool fallback) const
{
	const auto it = values_.find(key);
	if (it == values_.end())
		return fallback;
	return it->second == "1" || it->second == "true";
}

std::string Settings::getString(const std::string& key, const std::string& fallback) const
{
	const auto it = values_.find(key);
	return it == values_.end() ? fallback : it->second;
}

void Settings::set(const std::string& key, int value)
{
	values_[key] = std::to_string(value);
}

void Settings::set(const std::string& key, float value)
{
	std::ostringstream out;
	out << value;
	values_[key] = out.str();
}

void Settings::set(const std::string& key, bool value)
{
	values_[key] = value ? "1" : "0";
}

void Settings::set(const std::string& key, const std::string& value)
{
	values_[key] = value;
}

} // namespace app
