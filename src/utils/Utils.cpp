#include "../../includes/Utils.hpp"

#include "../../includes/Utils.hpp"
#include <cctype>
#include <sstream>

std::string Utils::trim(const std::string &str)
{
    size_t start = 0;
    size_t end = str.size();

    while (start < end && std::isspace(str[start]))
        start++;

    while (end > start && std::isspace(str[end - 1]))
        end--;

    return str.substr(start, end - start);
}

std::vector<std::string> Utils::split(const std::string &str, char delimiter)
{
    std::vector<std::string> result;
    std::string token;
    std::stringstream ss(str);

    while (std::getline(ss, token, delimiter))
    {
        result.push_back(token);
    }

    return result;
}

bool Utils::isValidMethod(const std::string &method)
{
    return (method == "GET" ||
            method == "POST" ||
            method == "DELETE");
}

bool Utils::isValidHttpVersion(const std::string &version)
{
    return (version == "HTTP/1.1" || version == "HTTP/1.0");
}

std::string Utils::toLower(const std::string &str)
{
    std::string res = str;

    for (size_t i = 0; i < res.size(); i++)
        res[i] = std::tolower(res[i]);

    return res;
}

bool Utils::isNumeric(const std::string &str)
{
    if (str.empty())
        return false;

    for (size_t i = 0; i < str.size(); i++)
    {
        if (!std::isdigit(str[i]))
            return false;
    }

    return true;
}