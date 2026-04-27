#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <iostream>
#include <vector>
#include "Server.hpp"

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