# Simple Makefile for the 3D Bouncing Ball Platformer
#
# Linux:
#   sudo apt-get install freeglut3-dev   (Debian/Ubuntu)
#   make
#   ./platformer
#
# macOS:
#   make            (uses -framework GLUT -framework OpenGL)
#   ./platformer

CXX := g++
CXXFLAGS := -O2 -Wall -std=c++14
TARGET := platformer
SRC := main.cpp

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    LIBS := -framework GLUT -framework OpenGL
else
    LIBS := -lGL -lGLU -lglut
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
