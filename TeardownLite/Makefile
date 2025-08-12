APP_NAME=teardown
CXX?=g++
CXXFLAGS?=-O2 -Wall -Wextra -std=c++17 $(shell sdl2-config --cflags)
LDFLAGS?=$(shell sdl2-config --libs)
SRC=$(wildcard src/*.cpp)
OBJ=$(SRC:.cpp=.o)

all: $(APP_NAME)

$(APP_NAME): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(APP_NAME)

.PHONY: all clean