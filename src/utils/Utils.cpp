#include "Utils.hpp"
#include <fstream>
#include <sstream>

namespace Utils
{
	std::string trim(const std::string &str)
	{
		size_t first = str.find_first_not_of(" \t\n\r");
		if (first == std::string::npos)
			return "";
		size_t last = str.find_last_not_of(" \t\n\r");
		return str.substr(first, last - first + 1);
	}

	std::vector<std::string> split(const std::string &str, char delimiter)
	{
		std::vector<std::string> tokens;
		std::stringstream ss(str);
		std::string token;
		while (std::getline(ss, token, delimiter))
		{
			tokens.push_back(token);
		}
		return tokens;
	}

	std::string readFile(const std::string &path)
	{
		std::ifstream file(path.c_str());
		if (!file.is_open())
			return "";
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	bool fileExists(const std::string &path)
	{
		std::ifstream file(path.c_str());
		return file.good();
	}

	std::string getMimeType(const std::string &extension)
	{
		if (extension == "html" || extension == "htm")
			return "text/html";
		if (extension == "css")
			return "text/css";
		if (extension == "js")
			return "application/javascript";
		if (extension == "json")
			return "application/json";
		if (extension == "png")
			return "image/png";
		if (extension == "jpg" || extension == "jpeg")
			return "image/jpeg";
		if (extension == "gif")
			return "image/gif";
		if (extension == "ico")
			return "image/x-icon";
		return "application/octet-stream";
	}
}
