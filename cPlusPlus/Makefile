# 编译器
CXX = g++

# 编译选项
CXXFLAGS = -Wall -Wextra -std=c++11  # 或者 -std=c++20

# 目标文件
TARGET = logger

# 使用 wildcard 函数获取所有 .cpp 文件，并替换为 .o 文件
OBJS = $(patsubst %.cpp,%.o,$(wildcard *.cpp))

# 默认目标
all: $(TARGET)

# 链接目标文件生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lstdc++fs  # 添加 -lstdc++fs

# 编译源文件生成目标文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: all clean
clean: # 清理规则
	rm -f $(OBJS) $(TARGET)
