#include "Config.hpp"
#include <fstream>
#include <sstream>

// ConfigParser helper functions

namespace ConfigParser
{
	bool parseFile(const std::string &filename, Config &config)
	{
		(void)filename;
		(void)config;
		return true;
	}

	bool validateConfig(const Config &config)
	{
		(void)config;
		return true;
	}
}
