# 头文件目录
INCLUDES = -I./code/log -I./code/buffer -I./code/pool -I./code/server -I./code/sql -I./code/timer -I./code/tool -I./code -I./code/bufferpool

# 源文件
SRCS = code/timer/*.cpp code/log/*.cpp code/buffer/*.cpp code/pool/*.cpp code/server/*.cpp code/sql/*.cpp code/tool/*.cpp code/bufferpool/*.cpp code/protocol.cpp code/main.cpp

# 库文件
LIBS = -lmysqlcppconn -pthread -lcrypt -lssl -lcrypto
# 目标文件名
TARGET = server

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

# 生成DEBUG版本，注意源文件要在库文件之前，如果先链接库文件的话，此时源文件的符号还没有被解析，而链接器会认为库中没有需要的符号，从而跳过它们
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