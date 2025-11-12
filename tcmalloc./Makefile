.PHONY: all
all:bench test

test:test.cpp
	g++ -std=c++20 -O2 -pthread test.cpp -o test

bench:benchmark.cpp
	g++ -std=c++20 -O2 -pthread benchmark.cpp -o bench

# 用于性能分析的调试版本（包含符号信息）
bench_debug:benchmark.cpp
	g++ -std=c++20 -O2 -g -pthread -fno-omit-frame-pointer benchmark.cpp -o bench_debug

test_debug:test.cpp
	g++ -std=c++20 -O2 -g -pthread -fno-omit-frame-pointer test.cpp -o test_debug

.PHONY:clean
clean:
	rm -f test bench bench_debug test_debug perf.data* *.perf *.valgrind
