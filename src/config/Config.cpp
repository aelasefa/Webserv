#include "Config.hpp"

Config::Config() : _configFile("")
{
}

Config::Config(const std::string &configFile) : _configFile(configFile)
{
}

Config::Config(const Config &src)
{
	*this = src;
}

Config::~Config()
{
}

Config &Config::operator=(const Config &rhs)
{
	if (this != &rhs)
	{
		_configFile = rhs._configFile;
	}
	return *this;
}

void Config::parse(const std::string &configFile)
{
	_configFile = configFile;
}
