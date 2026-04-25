CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

NAME = webserv

SRC = src/server/main.cpp src/server/Webserv.cpp src/server/Socket.cpp

OBJ = $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all