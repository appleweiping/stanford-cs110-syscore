# CS110 assign1 — Amazon Reviews Search
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -g -O2
PROGS    = mkamazondb dbase_test amazon_search

all: $(PROGS)

mkamazondb: mkamazondb.o
	$(CXX) $(CXXFLAGS) $^ -o $@

dbase_test: dbase_test.o amazon.o
	$(CXX) $(CXXFLAGS) $^ -o $@

amazon_search: amazon_search.o amazon.o
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cc amazon.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: test
test: all
	./tests/run.sh

.PHONY: clean
clean:
	rm -f *.o $(PROGS) tests/*.db tests/*.kwi
