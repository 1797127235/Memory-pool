CC := g++
CXXFLAGS := -std=c++14 -pthread -Wall -O2 -DNDEBUG

SRCS := ThreadCache.cpp CentralCache.cpp PageCache.cpp Benchmark.cpp
OBJS := $(SRCS:.cpp=.o)

test: $(OBJS)
	$(CC) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) test
