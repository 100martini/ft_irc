# Program name
NAME = ircserv

# Compiler and flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# Source files
SRCDIR = .
SOURCES = main.cpp \
		  Server.cpp \
		  ServerCommands.cpp \
		  Client.cpp \
		  Channel.cpp

# Object files
OBJDIR = obj
OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)

# Header files
HEADERS = Server.hpp \
		  Client.hpp \
		  Channel.hpp

# Colors for output
GREEN = \033[0;32m
RED = \033[0;31m
YELLOW = \033[0;33m
NC = \033[0m # No Color

# Default target
all: $(NAME)

# Create object directory
$(OBJDIR):
	@mkdir -p $(OBJDIR)
	@echo "$(YELLOW)Creating object directory...$(NC)"

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS) | $(OBJDIR)
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(NAME): $(OBJECTS)
	@echo "$(YELLOW)Linking $(NAME)...$(NC)"
	@$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(NAME)
	@echo "$(GREEN)$(NAME) compiled successfully!$(NC)"

# Clean object files
clean:
	@if [ -d "$(OBJDIR)" ]; then \
		echo "$(RED)Removing object files...$(NC)"; \
		rm -rf $(OBJDIR); \
	fi

# Clean everything
fclean: clean
	@if [ -f "$(NAME)" ]; then \
		echo "$(RED)Removing $(NAME)...$(NC)"; \
		rm -f $(NAME); \
	fi

# Rebuild everything
re: fclean all

# Debug build (optional)
debug: CXXFLAGS += -g -DDEBUG
debug: re

# Check for memory leaks with valgrind (optional)
valgrind: $(NAME)
	valgrind --leak-check=full --show-leak-kinds=all ./$(NAME)

# Help target (optional)
help:
	@echo "Available targets:"
	@echo "  all      - Build the program"
	@echo "  clean    - Remove object files"
	@echo "  fclean   - Remove object files and executable"
	@echo "  re       - Rebuild everything"
	@echo "  debug    - Build with debug flags"
	@echo "  valgrind - Run with valgrind (requires valgrind)"
	@echo "  help     - Show this help message"

# Phony targets
.PHONY: all clean fclean re debug valgrind help

# Dependency tracking (optional but useful)
-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS) | $(OBJDIR)
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@