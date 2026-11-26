CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra

HEADERS := $(wildcard codigos/*.hpp)

.PHONY: all test clean

all: bin/detector bin/wc2clf

bin/detector: codigos/main.cpp codigos/parser.cpp $(HEADERS) | bin
	$(CXX) $(CXXFLAGS) codigos/main.cpp codigos/parser.cpp -o $@

bin/wc2clf: codigos/wc2clf.cpp codigos/parser.cpp codigos/parser.hpp | bin
	$(CXX) $(CXXFLAGS) codigos/wc2clf.cpp codigos/parser.cpp -o $@

bin/tests: tests/test_sketches.cpp codigos/parser.cpp $(HEADERS) | bin
	$(CXX) $(CXXFLAGS) tests/test_sketches.cpp codigos/parser.cpp -o $@

test: bin/tests
	./bin/tests

bin:
	mkdir -p bin

clean:
	rm -rf bin
