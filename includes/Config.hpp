#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include "Server.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

enum State {
    NONE,
    SERVER,
    LOCATION
};
class Config {
public:
    std::vector<Server> servers;
};

#endif