#include "BufferPool.h"

// BufferDeleter
BufferPool::BufferDeleter::BufferDeleter(BufferPool* p)
  : pool(p)
{

}

void BufferPool::BufferDeleter::operator()(char* buf) const {
  if (pool && buf) {  // 绑定了，调用release归还
    pool->release(buf);
  }
  else if (buf) { // 未绑定，直接释放
    delete[] buf;
  }
}


// BufferPool
BufferPool::BufferPool(size_t buffer_size, size_t initial_count)
  : buffer_size_(buffer_size), stop_cleaner_(false)
{
  // 创建初始缓冲区
  for (size_t i=0; i<initial_count; ++i) {
    char* buf = createBuffer();
    pool_.push_back(buf); // 添加到池
    last_used_[buf] = std::chrono::steady_clock::now(); // 设置初始时间
  }
  // 启动超时清除线程
  startCleanerThread();
}

BufferPool::~BufferPool() {
  stop_cleaner_.store(true);  // 停止清除线程
  // 等待线程结束
  if (cleaner_thread_.joinable()) {
    cleaner_thread_.join();
  }
  // 释放所有缓冲区
  std::lock_guard<std::mutex> lock(mutex_);
  for (char* buf : pool_) {
    delete[] buf;
  }
  pool_.clear();
  last_used_.clear();
}

// 获取单例对象
BufferPool& BufferPool::getInstance() {
  static BufferPool instance(2048, 10);
  return instance;
}

// 获取缓冲区大小
size_t BufferPool::getBufferSize() {
  return buffer_size_;
}

// 申请缓冲区（返回智能指针，自动归还）
std::shared_ptr<char[]> BufferPool::acquire() {
  std::lock_guard<std::mutex> lock(mutex_);
  // 池为空，动态创建新缓冲区
  if (pool_.empty()) {
    return std::shared_ptr<char[]>(createBuffer(), BufferDeleter(this));
  }
  // 池不为空，从池中获取一个缓冲区
  char* buf = pool_.back();
  pool_.pop_back();
  
  return std::shared_ptr<char[]>(buf, BufferDeleter(this));
}

// 归还缓冲区（由智能指针的删除器调用）
void BufferPool::release(char* buf) {
  if (!buf) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  // 更新最后使用时间
  last_used_[buf] = std::chrono::steady_clock::now();
  pool_.push_back(buf); // 将缓冲区放回池中
}

// 创建新缓冲区
char* BufferPool::createBuffer() {
  return new char[buffer_size_];
}

// 启动超时清除线程，定期清除长时间未使用的缓冲区
void BufferPool::startCleanerThread() {
  cleaner_thread_ = std::thread([this]() {
    while (!(stop_cleaner_.load())) {
      // 每十分钟检查一次
      std::this_thread::sleep_for(std::chrono::minutes(10));
      cleanExpiredBuffers();
    }
  });
}

// 清除过期缓冲区
void BufferPool::cleanExpiredBuffers() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto now = std::chrono::steady_clock::now();
  for (auto it=pool_.begin(); it!=pool_.end(); ) {
    char* buf = *it;
    auto last_used_time = last_used_[buf];  // 最后使用时间
    // 计算时间差（秒）
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_used_time).count();
    if (duration > 3600) {  // 超过 1 小时（3600秒）
      last_used_.erase(buf);  // 从 last_used_中去除
      it = pool_.erase(it);   // 从池中去除，并指向下一个缓冲区
      delete[] buf;           // 释放空间
    }
    else {  // 没超时
      ++it; // 遍历下一个缓冲区
    }
  }
}