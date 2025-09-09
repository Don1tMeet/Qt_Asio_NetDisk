#pragma once


#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

// 对epoll的包装
class Epoller {
 public:
  explicit Epoller (int max_event = 1024);
  ~Epoller();

  bool addFd(int fd, uint32_t events);  // 给fd关联事件events

  bool modFd(int fd, uint32_t events);  // 将fd关联的事件修改events

  bool delFd(int fd);   // 从epoll中删除fd

  int wait(int timeout_ms = -1);      // 等待epoll事件就绪，返回就绪数量，超时时间为timeout_ms

  int getEventFd(size_t i) const;     // 返回第i个epoll时间关联的文件描述符

  uint32_t getEvents(size_t i) const; // 返回第i个文件描述符的关联事件

  size_t getEventVecSize();

 private:
  int epollfd_ = -1;   // epoll 文件描述符
  std::vector<struct epoll_event> event_vec_;  // 存储epoll事件的向量

};
