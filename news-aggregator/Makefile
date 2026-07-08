# CS110 assign5 — RSS News Feed Aggregator
CXX        = g++
CXXFLAGS   = -std=c++17 -Wall -Wextra -Werror -g -O2 -pthread $(shell xml2-config --cflags)
LDLIBS     = $(shell xml2-config --libs)

all: aggregate

aggregate: aggregate.o fetch.o xml-utils.o http.o thread-pool.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

aggregate.o: aggregate.cc fetch.h xml-utils.h thread-pool.h semaphore.h
fetch.o: fetch.cc fetch.h http.h
xml-utils.o: xml-utils.cc xml-utils.h fetch.h
http.o: http.cc http.h
thread-pool.o: thread-pool.cc thread-pool.h semaphore.h

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: test
test: aggregate
	./tests/run.sh

.PHONY: clean
clean:
	rm -f *.o aggregate
