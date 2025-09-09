#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

using buffer_shared_ptr = std::shared_ptr<char[]>;

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 后续使用自定义高性能Buffer，不使用原始char数组 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// 缓冲区池（线程安全）
class BufferPool {
 public:
  // 删除器，调用BufferPool::release
  struct BufferDeleter {
    BufferPool* pool{ nullptr };
    BufferDeleter() = default;
    BufferDeleter(BufferPool* p = nullptr); // 绑定到对应缓冲区池

    // 定义拷贝和移动语句
    BufferDeleter(const BufferDeleter&) = default;  // 使用默认，因为该类只有一个指针类，直接拷贝即可
    BufferDeleter& operator=(const BufferDeleter&) = default;
    BufferDeleter(BufferDeleter&&) = default;
    BufferDeleter& operator=(BufferDeleter&&) = default;

    void operator()(char* buf) const;       // 当shared_ptr销毁时，会被调用
  };

 public:
  BufferPool(size_t buffer_size = 8192, size_t initial_count = 100);
  ~BufferPool();

  static BufferPool& getInstance();
  size_t getBufferSize();
  // 申请缓冲区（返回智能指针，自动归还）
  std::shared_ptr<char[]> acquire();
  // 归还缓冲区（由智能指针的删除器调用）
  void release(char* buf);

 private:
  // 创建新缓冲区
  char* createBuffer();
  // 启动超时清除线程，定期清除长时间未使用的缓冲区
  void startCleanerThread();
  // 清除过期缓冲区
  void cleanExpiredBuffers();

 private:
  size_t buffer_size_{ 0 };     // 单个缓冲区大小
  std::vector<char*> pool_;     // 缓冲区池
  std::unordered_map<char*, std::chrono::steady_clock::time_point> last_used_;  // 记录缓冲区最后使用时间
  std::mutex mutex_;
  std::thread cleaner_thread_;  // 超时清除线程
  std::atomic<bool> stop_cleaner_{ false };   // 停止清除线程的标志
};
