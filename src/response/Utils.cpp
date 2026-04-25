#include <iostream>
#include <string>
#include <fstream>
#include "../../includes/Response.hpp"
#include <exception>

std::string readFile(std::string path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Error: could not open file");
    std::string line;
    std::string result;
    while (std::getline(file, line)) {
        result += line + "\n";
    }
    return result;
}
