# **************************************************************************** #
#                                 VARIABLES                                    #
# **************************************************************************** #

NAME		= webserv
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98
INC			= -I includes

# **************************************************************************** #
#                                 SOURCES                                      #
# **************************************************************************** #

SRC_DIR		= src
OBJ_DIR		= obj

SRCS		= $(SRC_DIR)/main.cpp \
			  $(SRC_DIR)/server/Server.cpp \
			  $(SRC_DIR)/server/Socket.cpp \
			  $(SRC_DIR)/server/Client.cpp \
			  $(SRC_DIR)/server/Webserv.cpp \
			  $(SRC_DIR)/config/Config.cpp \
			  $(SRC_DIR)/config/ConfigParser.cpp \
			  $(SRC_DIR)/http/Request.cpp \
			  $(SRC_DIR)/http/Response.cpp \
			  $(SRC_DIR)/http/Router.cpp \
			  $(SRC_DIR)/http/StatusCodes.cpp \
			  $(SRC_DIR)/cgi/CGI.cpp \
			  $(SRC_DIR)/utils/Utils.cpp \
			  $(SRC_DIR)/utils/Logger.cpp \
			  $(SRC_DIR)/bonus/Cookie.cpp \
			  $(SRC_DIR)/bonus/Session.cpp \
			  $(SRC_DIR)/bonus/Multipart.cpp

OBJS		= $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# **************************************************************************** #
#                                 HEADERS                                      #
# **************************************************************************** #

HEADERS		= includes/Server.hpp \
			  includes/Config.hpp \
			  includes/Socket.hpp \
			  includes/Client.hpp \
			  includes/Request.hpp \
			  includes/Response.hpp \
			  includes/Router.hpp \
			  includes/CGI.hpp \
			  includes/Utils.hpp \
			  includes/Logger.hpp \
			  includes/Webserv.hpp

# **************************************************************************** #
#                                 COLORS                                       #
# **************************************************************************** #

GREEN		= \033[0;32m
YELLOW		= \033[0;33m
RED			= \033[0;31m
NC			= \033[0m

# **************************************************************************** #
#                                 RULES                                        #
# **************************************************************************** #

all: $(NAME)

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(GREEN)✔ $(NAME) compiled successfully!$(NC)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@
	@echo "$(YELLOW)Compiling: $<$(NC)"

clean:
	@rm -rf $(OBJ_DIR)
	@echo "$(RED)✔ Object files removed!$(NC)"

fclean: clean
	@rm -f $(NAME)
	@echo "$(RED)✔ $(NAME) removed!$(NC)"

re: fclean all

.PHONY: all clean fclean re
