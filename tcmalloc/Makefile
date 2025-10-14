CXX = g++
CXXFLAGS = -std=c++11 -pthread -Wall -g

SOURCES = ThreadCache.cpp CentralCache.cpp PageCache.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# 测试申请功能
test_alloc: test.cpp $(OBJECTS)
	$(CXX) $(CXXFLAGS) test.cpp $(OBJECTS) -o test_alloc

# 测试归还功能
test_free: test_free.cpp $(OBJECTS)
	$(CXX) $(CXXFLAGS) test_free.cpp $(OBJECTS) -o test_free

# 编译所有测试
all: test_alloc test_free

# 编译目标文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 运行申请测试
run_alloc: test_alloc
	./test_alloc

# 运行归还测试
run_free: test_free
	./test_free

# 清理
clean:
	rm -f *.o test_alloc test_free

.PHONY: all clean run_alloc run_free
