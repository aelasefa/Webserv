#include "Router.hpp"

Router::Router() : _root("./www")
{
}

Router::Router(const Router &src)
{
	*this = src;
}

Router::~Router()
{
}

Router &Router::operator=(const Router &rhs)
{
	if (this != &rhs)
	{
		_root = rhs._root;
	}
	return *this;
}

Response Router::route(const Request &request)
{
	(void)request;
	Response response;
	return response;
}
