CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

NAME = webserv

SRC = src/main.cpp src/server/Webserv.cpp  src/server/Client.cpp \
	src/parsing/Request.cpp src/parsing/ConfigParser.cpp src/http/MethodHandler.cpp \
	src/http/FileHandler.cpp src/response/Response.cpp src/utils/Utils.cpp \
	src/CGI/CGI.cpp src/routing/routing.cpp src/session/SessionManager.cpp

OBJ = $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all