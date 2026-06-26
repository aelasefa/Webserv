#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <string>
#include <vector>
#include <sstream>

class StringUtils
{
public:
    static std::string trim(const std::string &str);
    static std::vector<std::string> split(const std::string &str, char delimiter);
    static std::string toLower(const std::string &str);
    static bool isNumeric(const std::string &str);
    static std::string htmlEscape(const std::string &str);

    template <typename T>
    static std::string toString(T value)
    {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }
};

#endif
