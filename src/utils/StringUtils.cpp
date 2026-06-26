#include "../../includes/StringUtils.hpp"
#include <cctype>
#include <sstream>

std::string StringUtils::trim(const std::string &str)
{
    size_t start = 0;
    size_t end = str.size();

    while (start < end && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r' || str[start] == '\n'))
        start++;

    while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r' || str[end - 1] == '\n'))
        end--;

    return str.substr(start, end - start);
}

std::vector<std::string> StringUtils::split(const std::string &str, char delimiter)
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

std::string StringUtils::toLower(const std::string &str)
{
    std::string res = str;

    for (size_t i = 0; i < res.size(); i++)
        res[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(res[i])));

    return res;
}

bool StringUtils::isNumeric(const std::string &str)
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

std::string StringUtils::htmlEscape(const std::string &str)
{
    std::string out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i)
    {
        switch (str[i])
        {
            case '&': out += "&amp;";  break;
            case '<': out += "&lt;";   break;
            case '>': out += "&gt;";   break;
            case '"': out += "&quot;"; break;
            default:  out += str[i];     break;
        }
    }
    return out;
}
