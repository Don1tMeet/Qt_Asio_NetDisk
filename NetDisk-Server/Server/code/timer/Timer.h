#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <functional>
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <sys/epoll.h>

// 类型别名
using TimeoutCallBack = std::function<void()>;      //超时事件回调函数
using Clock = std::chrono::high_resolution_clock;   //高精度时钟对象
using MS = std::chrono::milliseconds;               //毫秒级单位时间对象
using TimeStamp = Clock::time_point;                //时间戳，用于返回当前时间


// 定时器节点
struct TimerNode {
  int id;               // 定时器id
  TimeStamp expires;    // 到期时间
  TimeoutCallBack cb;   // 回调函数

  // 重载 < 用于堆排序
  bool operator<(const TimerNode &t) {
    return expires < t.expires;
  }
};


class Timer {
 public:
  Timer();
  ~Timer();

  int adjust(int id, int add_time);  // 传入定时器id，将其时间延迟到new_expires
  int add(int id, int timeout, const TimeoutCallBack &cb);  // 添加定时器
  int doing(int id);  // 传入定时器id，执行该定时器回调函数，并从堆中删除
  void clear();       // 清空堆
  int tick();         // 循环清楚全部超时定时器
  int pop();          // 删除堆的堆顶
  int getNextTick();  // 返回最近超时的定时器的剩余时间

 private: // 堆的内部操作
  bool __del(size_t i);                 // 删除堆中指定节点
  bool __siftup(size_t i);              // 上移指定节点
  bool __siftdown(size_t i, size_t n);  // 下移指定节点
  int __swap_node(size_t i, size_t j);  // 交换两个节点

 private:
  std::vector<TimerNode> heap_;
  std::unordered_map<int ,size_t> index_map_; // 定时器id在堆中下标的映射

  std::mutex mutex_;
};
