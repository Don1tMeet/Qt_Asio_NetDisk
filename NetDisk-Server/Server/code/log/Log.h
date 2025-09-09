#pragma once

#include <mutex>
#include <thread>
#include <string>
#include <sys/time.h>
#include <sys/stat.h>
#include <cstring>
#include <cassert>
#include <cstdarg>
#include "BlockDeque.h"
#include "Buffer.h"


class Log {
 public:
  void init(int level, const char* path = "./logfile", const char* suffix =".log", int max_queue_capacity = 1024);

  static Log* getInstance();      // 获取日记类的唯一单例
  static void flushLogThread();   // 刷新日志线程，使线程异步写入日志，用于传递给线程的运行函数，构造线程

  void write(int level, const char *format,...);  // 传递日志级别（0:debug、1:info、2:warn、3:error），格式化字符串，将该字符串写入日记文件
  void flush();                   // 将文件指针中的数据写入系统内核，存入文件

  int getLevel();                 // 获取当前日志级别，0:debug、1:info、2:warn、3：error，低于这个级别的日记不会被写入，如为3，怎0 1 2 都不会被记录
  void setLevel(int level);       // 设置当前日志级别，0:debug、1:info、2:warn、3：error
  
  //日志系统是否开始
  bool isOpen() { return is_open_; }

 private:
  Log();  // 单例模式
  virtual ~Log();
  void __appendLogLevelTitle(int level);
  void __asyncWrite();                    // 异步写入

 private:
  static const int LOG_PATH_LEN = 256;    // 日志保存路径最大长度
  static const int LOG_NAME_LEN = 256;    // 日志名最大长度
  static const int MAX_LINES = 50000;     // 单个日志文件记录最大行数

 private:
  const char* path_;      // 日志文件路径
  const char* suffix_;    // 日志文件后缀名

  int max_lines_;         // 日志文件最大行数
  int line_count_;        // 当前日志文件行数
  int to_day_;            // 当前日志文件日期

  int level_;             // 日志级别
  
  bool is_async_;         // 是否异步写入，1则为异步
  bool is_open_;          // 是否开始日志
  
  FILE* fp_;              // 日志文件指针
  Buffer buff_;           // 日志缓存区
  std::unique_ptr<BlockDeque<std::string>> deque_;    // 阻塞队列智能指针，内部存储string
  std::unique_ptr<std::thread> write_thread_;         // 异步写入日志线程智能指针
  std::mutex mtx_;
};


// 统一调用写入日志宏定义，输入日记等级（0:debug、1:info、2:warn、3：error），和格式化字符串，写入日记
#define LOG_BASE(level, format, ...) \
  do {\
    Log* log = Log::getInstance();\
    if (log->isOpen() && log->getLevel() <= level) {\
      log->write(level, format, ##__VA_ARGS__); \
      log->flush();\
    }\
  } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);
