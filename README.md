# Webserv

A lightweight HTTP server implementation in C++98, following the 42 School curriculum.

## Description

Webserv is a custom HTTP/1.1 server written in C++98. It handles multiple client connections, serves static files, processes CGI scripts, and supports various HTTP methods.

## Features

- HTTP/1.1 compliant
- Multiple virtual server support
- Static file serving
- CGI execution (PHP, Python, etc.)
- File upload handling
- Custom error pages
- Session and cookie management (bonus)

## Building

```bash
make        # Build the project
make clean  # Remove object files
make fclean # Remove object files and executable
make re     # Rebuild the project
```

## Usage

```bash
./webserv [config_file]
```

If no configuration file is specified, `conf/default.conf` will be used.

## Configuration

The server configuration is defined in a `.conf` file. See `conf/default.conf` for an example.

## Project Structure

```
webserv/
├── Makefile
├── README.md
├── conf/           # Configuration files
├── includes/       # Header files
├── src/            # Source files
│   ├── server/     # Server core components
│   ├── config/     # Configuration parsing
│   ├── http/       # HTTP request/response handling
│   ├── cgi/        # CGI execution
│   ├── utils/      # Utility functions
│   └── bonus/      # Bonus features
└── www/            # Web root directory
```

## Authors

- 42 Student

## License

This project is part of the 42 School curriculum.
