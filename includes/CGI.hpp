#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "Request.hpp"
#include "Response.hpp"

class CGI
{
	private:
		std::string _scriptPath;
		std::string _interpreter;

	public:
		CGI();
		~CGI();

		void setScriptPath(const std::string& path);
		void setInterpreter(const std::string& interpreter);

		std::string execute(const Request& request);
};

#endif