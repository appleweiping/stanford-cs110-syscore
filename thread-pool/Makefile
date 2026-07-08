# CS110 assign6 — ThreadPool
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -g -O2 -pthread

all: tptest

tptest: tptest.o thread-pool.o
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cc thread-pool.h semaphore.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: test
test: tptest
	./tptest

.PHONY: clean
clean:
	rm -f *.o tptest
