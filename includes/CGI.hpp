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
#define CGI_TIMEOUT 30

struct CGIState {
    pid_t pid;
    int outputFd;
    time_t startTime;
    std::string result;
    bool running;
	int status;
	int inputFd;
	size_t written;
	std::string requestBody;

	CGIState() :
		pid(-1),
		outputFd(-1),
		startTime(0),
		running(false),
		status(0),
		inputFd(-1),
		written(0)
	{}
};

class Server;

class CGI {
	private:
		std::string _scriptPath;
		std::string _interpreter;
	public:
		CGI();
		~CGI();

		void setScriptPath(const std::string& path);
		void setInterpreter(const std::string& interpreter);
		std::vector<std::string> setEnv(const Request& request, const Server& server);
		void start(const Request& request, CGIState& state, const Server& server);
		bool update(CGIState &state);
};

class CGIHandler {
	public:
		static Response buildResponse(const std::string& cgiOutput);
};

#endif