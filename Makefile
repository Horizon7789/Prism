# =========================================
# PRISM Makefile — Modular Build
# =========================================

# Compiler
# Compiler
CC = gcc

CFLAGS = -Wall -Wextra -O2 -g -std=c11 -DWEB -U_FORTIFY_SOURCE

LIBS = -lcurl -ljson-c

INCLUDES = -Icore -Iplanes -Iparser -Icontrollers -Ilearning -Igraph -Ireasoning -Ispeech -Istorage

# Source directories
SRC_DIRS = core planes parser controllers learning graph reasoning speech storage

# Collect all .c files
SOURCES = $(shell find $(SRC_DIRS) -name "*.c")
OBJECTS = $(SOURCES:.c=.o)

# Output binary
TARGET = $(HOME)/prism_ai

# =========================================
# Default build
# =========================================
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LIBS)

# =========================================
# Compile each .c to .o
# =========================================
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# =========================================
# Run PRISM
# =========================================
run: $(TARGET)
	$(TARGET)

# =========================================
# Clean build artifacts
# =========================================
clean:
	rm -f $(OBJECTS) $(TARGET)

# =========================================
# Rebuild from scratch
# =========================================
rebuild: clean all

# =========================================
# Debug build
# =========================================
debug: CFLAGS := -Wall -Wextra -g -std=c11 -O0
debug: rebuild

.PHONY: all run clean rebuild debug