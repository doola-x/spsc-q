CXX      ?= clang++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pthread

build: main

main: src/main.cpp include/spsc/queue.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o main

bench: bench/bench_queue.cpp include/spsc/queue.hpp
	$(CXX) $(CXXFLAGS) bench/bench_queue.cpp -o bench_queue

clean:
	rm -f main bench_queue

.PHONY: build bench clean
