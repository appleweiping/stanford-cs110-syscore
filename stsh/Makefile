# CS110 assign4 — stsh (Stanford Shell)
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -g -O0

all: stsh

stsh: stsh.o stsh-parse.o
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cc stsh-parse.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: test
test: stsh
	./tests/run.sh

.PHONY: clean
clean:
	rm -f *.o stsh
