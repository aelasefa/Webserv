CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

NAME = webserv

# [FIX CRIT-1] Replaced monolithic Utils.cpp with four specialized utility modules.
# Added  : StringUtils.cpp, FileUtils.cpp, HttpUtils.cpp, NetworkUtils.cpp.
# Removed: MethodHandler.cpp (dead code), Utils.cpp (fully superseded).
SRC = src/main.cpp src/server/Webserv.cpp src/server/Client.cpp \
	src/parsing/Request.cpp src/parsing/ConfigParser.cpp \
	src/http/FileHandler.cpp src/response/Response.cpp \
	src/utils/StringUtils.cpp src/utils/FileUtils.cpp \
	src/utils/HttpUtils.cpp src/utils/NetworkUtils.cpp \
	src/CGI/CGI.cpp src/routing/routing.cpp \
	src/session/SessionManager.cpp src/http/MimeTypes.cpp

OBJ = $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all