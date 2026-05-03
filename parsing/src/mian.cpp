#include <iostream>
#include <map>
#include <vector>
#include <string>
#include "../../include/config/ConfigParser.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: ./webserv config.conf" << std::endl;
        return 1;
    }

    ConfigParser parser;
    Config config = parser.parse(argv[1]);

    if (config.servers.empty()) {
        std::cout << "No servers parsed from config." << std::endl;
        return 0;
    }

    for (size_t i = 0; i < config.servers.size(); i++) {
        std::cout << "\n================ SERVER " << i << " ================\n";
        std::cout << "listen: " << config.servers[i].listen << std::endl;
        std::cout << "host: " << config.servers[i].host << std::endl;
        std::cout << "server_name: " << config.servers[i].server_name << std::endl;
        std::cout << "root: " << config.servers[i].root << std::endl;
        std::cout << "index: " << config.servers[i].index << std::endl;
        std::cout << "client_max_body_size: " << config.servers[i].client_max_body_size << std::endl;

        // Error pages
        std::cout << "\n--- ERROR PAGES ---\n";
        if (config.servers[i].error_pages.empty()) {
            std::cout << "(none)\n";
        } else {
            for (std::map<int, std::string>::iterator it = config.servers[i].error_pages.begin();
                 it != config.servers[i].error_pages.end(); ++it) {
                std::cout << it->first << " -> " << it->second << std::endl;
            }
        }

        // Locations
        std::cout << "\n--- LOCATIONS (" << config.servers[i].locations.size() << ") ---\n";
        if (config.servers[i].locations.empty()) {
            std::cout << "(none)\n";
        } else {
            for (size_t j = 0; j < config.servers[i].locations.size(); j++) {
                const Location& loc = config.servers[i].locations[j];
                std::cout << "\n  Location path: " << loc.path << std::endl;
                
                if (!loc.root.empty())
                    std::cout << "  root: " << loc.root << std::endl;
                if (!loc.index.empty())
                    std::cout << "  index: " << loc.index << std::endl;
                if (!loc.autoindex.empty())
                    std::cout << "  autoindex: " << loc.autoindex << std::endl;
                if (!loc.redirect.empty()) {
                    std::cout << "  redirect: " << loc.redirect << std::endl;
                }
                if (!loc.alias.empty())
                    std::cout << "  alias: " << loc.alias << std::endl;
                
                // Methods
                if (!loc.methods.empty()) {
                    std::cout << "  methods: ";
                    for (size_t k = 0; k < loc.methods.size(); k++) {
                        if (k > 0) std::cout << ", ";
                        std::cout << loc.methods[k];
                    }
                    std::cout << std::endl;
                }
                
                // CGI paths
                if (!loc.cgi_path.empty()) {
                    std::cout << "  cgi_path:";
                    for (size_t k = 0; k < loc.cgi_path.size(); k++)
                        std::cout << " " << loc.cgi_path[k];
                    std::cout << std::endl;
                }
                
                // CGI extensions
                if (!loc.cgi_ext.empty()) {
                    std::cout << "  cgi_ext:";
                    for (size_t k = 0; k < loc.cgi_ext.size(); k++)
                        std::cout << " " << loc.cgi_ext[k];
                    std::cout << std::endl;
                }
            }
        }
        std::cout << std::endl;
    }

    return 0;
}