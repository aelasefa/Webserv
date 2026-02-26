#include <string>
#include <map>
#include <ctime>

class Session
{
public:
	Session();
	Session(const std::string &id);
	Session(const Session &src);
	~Session();
	Session &operator=(const Session &rhs);

	std::string getId() const;
	void set(const std::string &key, const std::string &value);
	std::string get(const std::string &key) const;
	bool isExpired() const;

private:
	std::string _id;
	std::map<std::string, std::string> _data;
	time_t _createdAt;
	time_t _expiresAt;
};

Session::Session() : _id(""), _createdAt(time(0)), _expiresAt(time(0) + 3600)
{
}

Session::Session(const std::string &id) : _id(id), _createdAt(time(0)), _expiresAt(time(0) + 3600)
{
}

Session::Session(const Session &src)
{
	*this = src;
}

Session::~Session()
{
}

Session &Session::operator=(const Session &rhs)
{
	if (this != &rhs)
	{
		_id = rhs._id;
		_data = rhs._data;
		_createdAt = rhs._createdAt;
		_expiresAt = rhs._expiresAt;
	}
	return *this;
}

std::string Session::getId() const
{
	return _id;
}

void Session::set(const std::string &key, const std::string &value)
{
	_data[key] = value;
}

std::string Session::get(const std::string &key) const
{
	std::map<std::string, std::string>::const_iterator it = _data.find(key);
	if (it != _data.end())
		return it->second;
	return "";
}

bool Session::isExpired() const
{
	return time(0) > _expiresAt;
}
