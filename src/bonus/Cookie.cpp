#include <string>
#include <map>

class Cookie
{
public:
	Cookie();
	Cookie(const Cookie &src);
	~Cookie();
	Cookie &operator=(const Cookie &rhs);

	void set(const std::string &name, const std::string &value);
	std::string get(const std::string &name) const;
	std::string toHeader() const;

private:
	std::map<std::string, std::string> _cookies;
};

Cookie::Cookie()
{
}

Cookie::Cookie(const Cookie &src)
{
	*this = src;
}

Cookie::~Cookie()
{
}

Cookie &Cookie::operator=(const Cookie &rhs)
{
	if (this != &rhs)
	{
		_cookies = rhs._cookies;
	}
	return *this;
}

void Cookie::set(const std::string &name, const std::string &value)
{
	_cookies[name] = value;
}

std::string Cookie::get(const std::string &name) const
{
	std::map<std::string, std::string>::const_iterator it = _cookies.find(name);
	if (it != _cookies.end())
		return it->second;
	return "";
}

std::string Cookie::toHeader() const
{
	std::string header;
	std::map<std::string, std::string>::const_iterator it;
	for (it = _cookies.begin(); it != _cookies.end(); ++it)
	{
		header += it->first + "=" + it->second + "; ";
	}
	return header;
}
