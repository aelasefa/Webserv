#include <string>
#include <vector>
#include <map>

struct MultipartPart
{
	std::map<std::string, std::string> headers;
	std::string name;
	std::string filename;
	std::string content;
};

class Multipart
{
public:
	Multipart();
	Multipart(const std::string &boundary);
	Multipart(const Multipart &src);
	~Multipart();
	Multipart &operator=(const Multipart &rhs);

	void parse(const std::string &body);
	std::vector<MultipartPart> getParts() const;

private:
	std::string _boundary;
	std::vector<MultipartPart> _parts;
};

Multipart::Multipart() : _boundary("")
{
}

Multipart::Multipart(const std::string &boundary) : _boundary(boundary)
{
}

Multipart::Multipart(const Multipart &src)
{
	*this = src;
}

Multipart::~Multipart()
{
}

Multipart &Multipart::operator=(const Multipart &rhs)
{
	if (this != &rhs)
	{
		_boundary = rhs._boundary;
		_parts = rhs._parts;
	}
	return *this;
}

void Multipart::parse(const std::string &body)
{
	(void)body;
}

std::vector<MultipartPart> Multipart::getParts() const
{
	return _parts;
}
