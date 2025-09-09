# 头文件目录
INCLUDES = -I./code/bufferpool -I./code/epoller -I./code/equalizer -I./code/serverheap -I./code/tool -I./code
# 源文件
SRCS = code/bufferpool/*.cpp code/epoller/*.cpp code/equalizer/*.cpp code/serverheap/*.cpp code/tool/*.cpp code/main.cpp

# 库文件
LIBS = -pthread

# 目标文件名
TARGET = mainserver

# 编译器
CXX = g++

# 编译选项：公共
CXXFLAGS_COMMON = -Wall -Wextra -std=c++17 ${INCLUDES}

# 编译选项：DEBUG
CXXFLAGS_DEBUG = ${CXXFLAGS_COMMON} -g -O0 -DDEBUG

# 编译选项：RELEASE
CXXFLAGS_RELEASE = ${CXXFLAGS_COMMON} -O2 -DNDEBUG

# 生成两个版本
all: debug release

# 生成DEBUG版本
debug:
	$(CXX) ${CXXFLAGS_DEBUG} ${SRCS} -o ./bin/$(TARGET)_debug ${LIBS}

# 生成RELEASE版本
release:
	${CXX} ${CXXFLAGS_RELEASE} ${SRCS} -o ./bin/${TARGET}_release ${LIBS}

# 清除生成的文件
clean:
	rm -f ${TARGET}_debug ${TARGET}_release

# 伪目标：防止与同名文件冲突
.PHONY: debug release clean all