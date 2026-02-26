#include <string>
#include <map>

namespace StatusCodes
{
	std::string getStatusMessage(int code)
	{
		std::map<int, std::string> statusCodes;
		statusCodes[200] = "OK";
		statusCodes[201] = "Created";
		statusCodes[204] = "No Content";
		statusCodes[301] = "Moved Permanently";
		statusCodes[302] = "Found";
		statusCodes[304] = "Not Modified";
		statusCodes[400] = "Bad Request";
		statusCodes[401] = "Unauthorized";
		statusCodes[403] = "Forbidden";
		statusCodes[404] = "Not Found";
		statusCodes[405] = "Method Not Allowed";
		statusCodes[413] = "Payload Too Large";
		statusCodes[500] = "Internal Server Error";
		statusCodes[501] = "Not Implemented";
		statusCodes[502] = "Bad Gateway";
		statusCodes[503] = "Service Unavailable";
		statusCodes[505] = "HTTP Version Not Supported";

		std::map<int, std::string>::iterator it = statusCodes.find(code);
		if (it != statusCodes.end())
			return it->second;
		return "Unknown";
	}
}
