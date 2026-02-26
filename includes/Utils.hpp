#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

namespace Utils
{
	std::string trim(const std::string &str);
	std::vector<std::string> split(const std::string &str, char delimiter);
	std::string readFile(const std::string &path);
	bool fileExists(const std::string &path);
	std::string getMimeType(const std::string &extension);
}

#endif
