#include "WorkQue.h"
#include <cassert>

WorkQue::WorkQue(size_t thread_count) : is_close_(false) {
  assert(thread_count > 0);
  // 创建线程，这里创建分离式线程，简单，不用手动管理
  for (size_t i=0; i<thread_count; ++i) {
    threads_.emplace_back(std::thread([this](){
      while(true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(mtx_);
          cv_.wait(lock, [this]() { return is_close_ || !task_que_.empty(); });
          if (is_close_ && task_que_.empty()) {  // 线程池停止，并且所有任务都执行完
            return;
          }
          task = std::move(task_que_.front());
          task_que_.pop();
        }
        task();
      }
    }));
  }
}

WorkQue::~WorkQue() {
  close();
}

void WorkQue::close() {
  if (is_close_.load()) {
    return;
  }

  is_close_.store(true);  // 关闭线程池

  cv_.notify_all();       // 通知所有线程

  for (std::thread &th : threads_) {  // 等待所有线程执行完后关闭线程回收资源
    if (th.joinable()) {
      th.join();
    }
  }
}