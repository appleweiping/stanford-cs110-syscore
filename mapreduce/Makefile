# CS110 assign8 — MapReduce (optional / ungraded)
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -g -O2 -pthread
PROGS    = mr mapper reducer

all: $(PROGS)

mr: mr.o subprocess.o thread-pool.o
	$(CXX) $(CXXFLAGS) $^ -o $@

mapper: mapper.o
	$(CXX) $(CXXFLAGS) $^ -o $@

reducer: reducer.o
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: test
test: all
	./tests/run.sh

.PHONY: clean
clean:
	rm -f *.o $(PROGS)
	rm -rf tests/out
