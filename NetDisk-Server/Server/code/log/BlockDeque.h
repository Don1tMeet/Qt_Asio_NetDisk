#pragma once


#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>


template<class T>
class BlockDeque {
 public:
  explicit BlockDeque(size_t max_capacity = 1000);
  ~BlockDeque();

  bool empty();
  bool full();        // 返回阻塞队列是否满了
  size_t size();      // 返回队列当前元素数量
  size_t capacity();  // 返回队列可用空间

  void clear();
  void push_front(const T &item); // 添加元素，并唤醒一个消费者线程
  void push_back(const T &item);
  T front();
  T back();
  bool pop(T &item);              // 弹出队首，保存在item
  bool pop(T &item, int timeout); // 弹出队首，保存在item，非阻塞版
  
  void flush();   // 唤醒一个消费者线程
  
  void close();

 private:
  std::deque<T> deque_;
  size_t capacity_;
  std::mutex mtx_;
  std::condition_variable cv_consumer_;   // 消费者等待的条件变量
  std::condition_variable cv_producer_;   // 生产者等待的条件变量
  bool is_close_;
};




template<class T>
BlockDeque<T>::BlockDeque(size_t max_capacity) : capacity_(max_capacity), is_close_(false) {
  assert(max_capacity > 0);
}

template<class T>
BlockDeque<T>::~BlockDeque() {
  close();
}

template<class T>
void BlockDeque<T>::close() {
  {
    std::lock_guard<std::mutex> locker(mtx_);
    if (is_close_) {
      return;
    }
    deque_.clear();
    is_close_ = true;
  }
  cv_consumer_.notify_all();
  cv_producer_.notify_all();
};

template<class T>
void BlockDeque<T>::flush() {
  cv_consumer_.notify_one();
};

template<class T>
void BlockDeque<T>::clear() {
  std::lock_guard<std::mutex> locker(mtx_);
  deque_.clear();
}

template<class T>
T BlockDeque<T>::front() {
  std::lock_guard<std::mutex> locker(mtx_);
  return deque_.front();
}

template<class T>
T BlockDeque<T>::back() {
  std::lock_guard<std::mutex> locker(mtx_);
  return deque_.back();
}

template<class T>
size_t BlockDeque<T>::size() {
  std::lock_guard<std::mutex> locker(mtx_);
  return deque_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
  std::lock_guard<std::mutex> locker(mtx_);
  return capacity_;
}

template<class T>
void BlockDeque<T>::push_back(const T &item) {
  {
    std::unique_lock<std::mutex> locker(mtx_);
    // 只有队列未满时才添加
    cv_producer_.wait(locker, [this](){ return is_close_ || deque_.size() < capacity_; });
    if (is_close_) {
      return;
    }
    deque_.push_back(item);
  }
  cv_consumer_.notify_one();  //唤醒一个消费者线程去使用
}

template<class T>
void BlockDeque<T>::push_front(const T &item) {
  {
    std::unique_lock<std::mutex> locker(mtx_);
    // 只有队列未满时才添加
    cv_producer_.wait(locker, [this](){ return is_close_ || deque_.size() < capacity_; });
    if (is_close_) {
      return;
    }
    deque_.push_front(item);
  }
  cv_consumer_.notify_one();  //唤醒一个消费者线程去使用
}

template<class T>
bool BlockDeque<T>::empty() {
  std::lock_guard<std::mutex> locker(mtx_);
  return deque_.empty();
}

template<class T>
bool BlockDeque<T>::full(){
  std::lock_guard<std::mutex> locker(mtx_);
  return deque_.size() >= capacity_;
}

template<class T>
bool BlockDeque<T>::pop(T &item) {
  {
    std::unique_lock<std::mutex> locker(mtx_);
    // 只有队列非空时才能删除，如果关闭也唤醒
    cv_consumer_.wait(locker, [this](){ return is_close_ || !deque_.empty(); }); 
    if(is_close_){  //如果阻塞队列已经关闭，返回false
      return false;
    }
    //取出头部元素，并从队列中删除
    item = deque_.front();
    deque_.pop_front();
  }
  cv_producer_.notify_one();
  return true;
}

template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
  {
    std::unique_lock<std::mutex> locker(mtx_);
    // 只有队列非空时才能删除，如果关闭也唤醒
    if (cv_consumer_.wait_for(locker, std::chrono::seconds(timeout), [this](){ return is_close_ || !deque_.empty(); })) {
      if(is_close_){  //如果阻塞队列已经关闭，返回false
        return false;
      }
      //取出头部元素，并从队列中删除
      item = deque_.front();
      deque_.pop_front();
    }
    else {  // 超时
      return false;
    }
  }
  cv_producer_.notify_one();
  return true;
}