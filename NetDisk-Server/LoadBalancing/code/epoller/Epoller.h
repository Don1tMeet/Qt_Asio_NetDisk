#pragma once


#include <vector>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <unistd.h>
#include <sys/epoll.h>


class Epoller {
 public:
  explicit Epoller(uint32_t max_event = 1024);  // 传递最大epoll最大事件数，构造epoll，默认为1024
  ~Epoller();

  bool addFd(int fd, uint32_t events);    // 传递文件描述符fd，和事件events，将该文件描述符和事件关联，添加到epoll监控
  bool modFd(int fd, uint32_t events);    // 将文件描述符fd，关联的事件修改
  bool delFd(int fd);                     // 从epoll中删除文件描述符fd

  int wait(int timeoutMs = -1);           // 传递超时时间，等待epoll事件返回就绪文件描述符就绪数量，超时返回。默认-1，阻塞等待

  int getEventFd(size_t i) const;         // 返回第i个epool事件关联的文件描述符
  uint32_t getEvents(size_t i) const;     // 返回第i个事件类型

 private:
  int epoll_fd_{ -1 };  // epoll 文件描述符

  std::vector<struct epoll_event> events_;    // 存储epoll事件的vector
};
