# CS110 assign7 — HTTP Web Proxy and Cache
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -g -O2 -pthread

all: proxy

proxy: proxy.o http.o thread-pool.o
	$(CXX) $(CXXFLAGS) $^ -o $@

proxy.o: proxy.cc http.h thread-pool.h semaphore.h
http.o: http.cc http.h
thread-pool.o: thread-pool.cc thread-pool.h semaphore.h

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: test
test: proxy
	./tests/run.sh

.PHONY: clean
clean:
	rm -f *.o proxy
