TARGET = nvdaal-cli
SRC = main.cpp ../../Library/libNVDAAL.cpp
CXX = clang++
CXXFLAGS = -Wall -std=c++17 -framework IOKit -framework CoreFoundation -I../../Library

CXXFLAGS = -std=c++17 -Wall -I../../Library
LDFLAGS = -L../../Build -lNVDAAL -framework IOKit -framework CoreFoundation

all: nvdaal-cli test_mmu

nvdaal-cli: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test_mmu: test_mmu.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f nvdaal-cli test_mmu
