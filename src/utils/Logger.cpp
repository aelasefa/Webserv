#include "Logger.hpp"
#include <iostream>
#include <ctime>

Logger::Logger()
{
}

Logger::Logger(const Logger &src)
{
	(void)src;
}

Logger::~Logger()
{
}

Logger &Logger::operator=(const Logger &rhs)
{
	(void)rhs;
	return *this;
}

std::string Logger::_getTimestamp()
{
	time_t now = time(0);
	char buf[80];
	struct tm *timeinfo = localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", timeinfo);
	return std::string(buf);
}

void Logger::info(const std::string &message)
{
	std::cout << "[" << _getTimestamp() << "] [INFO] " << message << std::endl;
}

void Logger::error(const std::string &message)
{
	std::cerr << "[" << _getTimestamp() << "] [ERROR] " << message << std::endl;
}

void Logger::debug(const std::string &message)
{
	std::cout << "[" << _getTimestamp() << "] [DEBUG] " << message << std::endl;
}

void Logger::warning(const std::string &message)
{
	std::cout << "[" << _getTimestamp() << "] [WARNING] " << message << std::endl;
}
