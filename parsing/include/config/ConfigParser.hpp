#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

class ConfigParser {
public:
    Config parse(const std::string& filename);
};

#endif