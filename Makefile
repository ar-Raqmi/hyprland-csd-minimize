CXX = g++
CXXFLAGS = -shared -fPIC -std=c++23 -O2
INCLUDES = $(shell pkg-config --cflags hyprland)
TARGET = csd-minimize.so
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
