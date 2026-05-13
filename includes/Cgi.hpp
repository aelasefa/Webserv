#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdexcept>
#include "Response.hpp"

class Cgi
{
	private:
		std::string _scriptPath;
		std::string _interpreter;
	public:
		Cgi();
		~Cgi();

		void setScriptPath(const std::string& path);
		void setInterpreter(const std::string& interpreter);
		std::vector<std::string> setEnv(const Request& request);
		std::string execute(const Request& request);
};

class CgiHandler {
	public:
		static Response buildResponse(const std::string& cgiOutput);
};

#endif