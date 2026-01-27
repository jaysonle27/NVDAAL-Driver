TARGET = nvdaal-cli
SRC = main.cpp ../../Library/libNVDAAL.cpp
CXX = clang++
CXXFLAGS = -Wall -std=c++17 -framework IOKit -framework CoreFoundation -I../../Library

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
