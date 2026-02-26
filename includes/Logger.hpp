#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>

class Logger
{
public:
	static void info(const std::string &message);
	static void error(const std::string &message);
	static void debug(const std::string &message);
	static void warning(const std::string &message);

private:
	Logger();
	Logger(const Logger &src);
	~Logger();
	Logger &operator=(const Logger &rhs);

	static std::string _getTimestamp();
};

#endif
