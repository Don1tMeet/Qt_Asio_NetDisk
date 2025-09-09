#pragma once

#include <unistd.h>
#include <sys/uio.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>
#include <atomic>



class Buffer {
 public:
  Buffer(size_t buffer_size = 4096);
  Buffer(const Buffer &other) = delete;
  Buffer& operator=(const Buffer &other) = delete;
  Buffer(Buffer&& other) = delete;
  Buffer& operator=(Buffer &&other) = delete;
  ~Buffer() = default;

 public:  // 返回相关属性的函数
  size_t writeAbleBytes() const;    // 返回可写的字节数
  size_t readAbleBytes() const;     // 返回可写的字节数
  size_t prependAbleBytes() const;  // 预留空间的字节数

  const char* beginRead() const;    // 返回可读数据首地址
  char* beginWrite();               // 返回可写数据首地址
  const char* beginWriteConst() const;   // 返回可写数据首地址，const版
  void ensureWriteAble(size_t len); // 确保缓冲区还有len字节可用，有什么都不做，没有则拓展

 public:  // 修改相关属性的函数
  void hasWritten(size_t len);          // 标记已经写入了len字节，令写坐标增加len
  void retrieve(size_t len);            // 标记已经读取了len字节
  void retrieveUntil(const char *end);  // 将缓冲区已读取位置修改到end
  void retrieveAll();                   // 清空缓冲区
  std::string retrieveAllToStr();       // 将缓存区的全部数据转为string来返回，并清空

  char* peek();                         // 返回可读数据的首地址
  const char* peek() const;             // 返回可读数据的首地址，const版本
  std::string peekToStr(size_t len);    // 返回可读数据的前len字节的string，不标记
  std::string peekAllToStr();           // 返回所有可读数据的string，不标记

 public:  // 添加数据到缓冲区
  void append(const std::string &str);
  void append(const char* str, size_t len);
  void append(const void* data, size_t len);
  void append(const Buffer &buf);

  ssize_t readFd(int fd, int* save_errno);
  ssize_t writeFd(int fd, int* save_errno);

 private: // 内部函数
  char* __begin();              // 返回缓冲区首地址
  const char* __begin() const;
  void __makeSpace(size_t len); // 将缓冲区拓展len字节

 private: // 成员
  std::vector<char> buffer_;
  std::atomic<std::size_t> read_pos_;
  std::atomic<std::size_t> write_pos_;
};
