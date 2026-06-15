#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdexcept>
#include "Request.hpp"
#include "Response.hpp"
#include <sstream>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <ctime>

class CGI {
	private:
		std::string _scriptPath;
		std::string _interpreter;
	public:
		CGI();
		~CGI();

		void setScriptPath(const std::string& path);
		void setInterpreter(const std::string& interpreter);
		std::vector<std::string> setEnv(const Request& request);
		std::string execute(const Request& request);
};

class CGIHandler {
	public:
		static Response buildResponse(const std::string& cgiOutput);
};

#endif