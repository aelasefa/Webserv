CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

NAME = webserv

SRC = src/main.cpp src/Webserv.cpp #src/Socket.cpp

OBJ = $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all