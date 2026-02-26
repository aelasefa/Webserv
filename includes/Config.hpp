#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

class Config
{
public:
	Config();
	Config(const std::string &configFile);
	Config(const Config &src);
	~Config();
	Config &operator=(const Config &rhs);

	void parse(const std::string &configFile);

private:
	std::string _configFile;
};

#endif
