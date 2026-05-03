# Makefile for SSH Ice & Fire
CXX      := g++
CXXFLAGS := -pedantic-errors -std=c++11 -Wall -Wextra -O2
TARGET   := Frozen_Spark

# Object files
OBJS := main.o \
        game.o \
        utils.o \
        ranking.o \
        fixed_level.o \
        random_archive.o \
        terminal.o

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compilation rules with header dependencies

main.o: main.cpp config.h ranking.h fixed_level.h random_archive.h game.h utils.h
	$(CXX) $(CXXFLAGS) -c main.cpp

game.o: game.cpp game.h config.h terminal.h fixed_level.h random_archive.h
	$(CXX) $(CXXFLAGS) -c game.cpp

utils.o: utils.cpp utils.h ranking.h random_archive.h fixed_level.h config.h
	$(CXX) $(CXXFLAGS) -c utils.cpp

ranking.o: ranking.cpp ranking.h
	$(CXX) $(CXXFLAGS) -c ranking.cpp

fixed_level.o: fixed_level.cpp fixed_level.h
	$(CXX) $(CXXFLAGS) -c fixed_level.cpp

random_archive.o: random_archive.cpp random_archive.h
	$(CXX) $(CXXFLAGS) -c random_archive.cpp

terminal.o: terminal.cpp terminal.h
	$(CXX) $(CXXFLAGS) -c terminal.cpp

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Declare phony targets
.PHONY: all clean
