# 编译器
CXX := g++
# 编译选项（Release 版）
CXXFLAGS := -std=c++17 -O2 -DNDEBUG -march=native -Wall -Wextra
# 头文件目录
INCLUDES := -I.
# 链接选项
LDFLAGS :=

# 源文件（自动匹配当前目录下所有 .cpp）
SRC := $(wildcard *.cpp)
# 对应的目标文件
OBJ := $(SRC:.cpp=.o)

# 生成的可执行文件名
TARGET := main

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理
clean:
	rm -f $(OBJ) $(TARGET)

# 伪目标
.PHONY: all clean
