CXX = g++
CXXFLAGS = -shared -fPIC -std=c++23 -O2
INCLUDES = $(shell pkg-config --cflags hyprland)
TARGET = libcsd-minimize.so
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(TARGET)

install: all
	install -d $(DESTDIR)$(PREFIX)/lib
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/lib/

clean:
	rm -f $(TARGET)

.PHONY: all clean
