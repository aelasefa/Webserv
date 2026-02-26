#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>
#include "Request.hpp"
#include "Response.hpp"

class Router
{
public:
	Router();
	Router(const Router &src);
	~Router();
	Router &operator=(const Router &rhs);

	Response route(const Request &request);

private:
	std::string _root;
};

#endif
