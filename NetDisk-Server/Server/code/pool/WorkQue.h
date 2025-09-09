#pragma once

#include "ShortTaskTool.h"
#include "LongTaskTool.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <assert.h>
#include <thread>
#include <functional>

// 任务队列
class WorkQue {
 public:
  explicit WorkQue(size_t thread_count = 10);
  ~WorkQue();

  template<class F>
  void addTask(F &&task);
  void close();

 private:
  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> task_que_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::atomic<bool> is_close_{ false };
};

template <class F>
inline void WorkQue::addTask(F &&task) {
  if (is_close_.load()) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mtx_);
    task_que_.emplace(std::forward<F>(task));
  }
  cv_.notify_one();
}