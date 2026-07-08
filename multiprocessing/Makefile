# CS110 assign3 — All Things Multiprocessing
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -g -O0
PROGS    = subprocess-test pipeline-test farm

all: $(PROGS)

subprocess-test: subprocess-test.o subprocess.o
	$(CXX) $^ -o $@

pipeline-test: pipeline-test.o pipeline.o
	$(CXX) $^ -o $@

farm: farm.o
	$(CXX) $^ -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: test
test: all
	@echo "== subprocess-test =="; ./subprocess-test
	@echo "== pipeline-test (sort | uniq -c) =="; \
	  printf 'pear\napple\npear\nbanana\napple\npear\n' | ./pipeline-test
	@echo "== farm (factor 12 numbers across all CPUs) =="; \
	  printf '12\n97\n1000000007\n360\n1024\n17\n99991\n123456\n2\n42\n7919\n1048576\n' | ./farm

.PHONY: clean
clean:
	rm -f *.o $(PROGS)
