#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

class Utils
{
public:
    static std::string trim(const std::string &str);
    static std::vector<std::string> split(const std::string &str, char delimiter);
    std::string toString(int n);
    static bool isValidMethod(const std::string &method);
    static bool isValidHttpVersion(const std::string &version);

    static std::string toLower(const std::string &str);
    static bool isNumeric(const std::string &str);
};

#endif