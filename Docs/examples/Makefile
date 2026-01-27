CXX = clang++
CXXFLAGS = -std=c++17 -Wall -I../../Library
LDFLAGS = -L../../Build -lNVDAAL -framework IOKit -framework CoreFoundation

all: hello_compute

hello_compute: hello_compute.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f hello_compute
