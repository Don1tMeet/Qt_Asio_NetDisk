#include "Timer.h"


Timer::Timer() {
  heap_.reserve(64);
}

Timer::~Timer() {
  clear();
}

// 传入定时器id，将其时间延迟到当前系统时间+add_time毫秒
int Timer::adjust(int id, int add_time) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (heap_.empty() || index_map_.count(id) <= 0) {
    std::cerr << "timer id: " << id << " not exist" << std::endl;
    return -1;
  }
  heap_[index_map_[id]].expires = Clock::now() + MS(add_time);
  __siftdown(index_map_[id], heap_.size());

  return 0;
}

// 指定定时器id，初始超时时间，回调函数，将他加入到堆中
int Timer::add(int id, int timeout, const TimeoutCallBack &cb) {
  if (id < 0) {
    return -1;
  }
  size_t i{0};

  std::lock_guard<std::mutex> lock(mutex_);

  // 是否在映射表中，不在，则创建，在，则调整
  if(index_map_.count(id) == 0) {
    i = heap_.size();
    index_map_[id] = i;
    heap_.push_back({id, Clock::now() + MS(timeout), cb});
    __siftup(i); // 因为堆尾插入，需要向上调整
  } 
  else {
    i = index_map_[id];
    heap_[i].expires = Clock::now() + MS(timeout);
    heap_[i].cb = cb;
    if(!__siftdown(i, heap_.size())) {
      __siftup(i);
    } 
  }
  return 0;
}

// 传入定时器id，运行它的回调函数，并将其从堆中删除
int Timer::doing(int id) {
  std::lock_guard<std::mutex> lock(mutex_);

  if(heap_.empty() || index_map_.count(id) == 0) {
    return -1;
  }
  size_t i = index_map_[id]; //返回在堆中的下标
  heap_[i].cb();  // 执行回调函数
  __del(i);    //删除该节点

  return 0;
}

// 清空堆
void Timer::clear() {
  std::lock_guard<std::mutex> lock(mutex_);

  index_map_.clear();
  heap_.clear();
}

// 清除全部超时节点，返回清除的节点数
int Timer::tick() {
  int cnt = 0;
  while(!heap_.empty()) {
    TimerNode node = heap_.front();
    if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
      break; 
    }
    node.cb();  //调用回调函数，进行超时处理
    pop();      //将该结点删除
    ++cnt;
  }
  return cnt;
}

// 删除堆顶
int Timer::pop() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (heap_.empty()) {
    return -1;
  }
  __del(0);
  return 0;
}

// 返回最近超时的定时器剩余时间，堆为空返回-1，0表示已经超时，否则返回剩余多少时间超时
int Timer::getNextTick() {
  std::lock_guard<std::mutex> lock(mutex_);

  tick(); //先处理已经超时的
  if (heap_.empty()) {
    return -1;
  }
  size_t res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
  // 如果最近的定时器已经超时，返回0，否则返回剩余多少时间超时
  return std::max(res, static_cast<size_t>(0));
}

// 传递指定节点的下标，并将其删除,删除后并调整堆结构
bool Timer::__del(size_t i) {
  if (i >= heap_.size()) {
    return false;
  }
  // 将要删除的结点换到队尾，然后调整堆
  size_t n = heap_.size() - 1;  // 最后一个节点(也是修改后的大小)
  __swap_node(i, n);  //将其交换到最后节点
  
  if(!__siftdown(i, n)) { // 调整堆，由于不知道换过来的节点的大小，因此上下都测试一遍
    __siftup(i);
  }
  // 队尾元素删除
  index_map_.erase(heap_.back().id);   
  heap_.pop_back();
  return true;
}

// 传入定时器在堆数组中的下标，并根据它的超时时间剩余数，往上移，保持最小堆结构
bool Timer::__siftup(size_t i) {
  if (i >= heap_.size()) {
    return false;
  }
  while (i > 0) {
    size_t j = (i - 1) / 2; // 父节点
    if (heap_[j] < heap_[i]) {  // 当前节点大于父节点，不需要更新（小根堆）
      break;
    }
    __swap_node(i, j);  // 将其与父节点交换
    i = j;  // 更新下标指向父节点，继续往上调整
  }
  return true;
}

// 传入定时器在堆中的索引和堆长度，调整堆结构。
bool Timer::__siftdown(size_t i, size_t n) {
  if (i >= heap_.size() || n > heap_.size()) {
    return false;
  }
  size_t pre_i = i;       // 记录传入的索引，用于判断是否交换过
  size_t j = i * 2 + 1;   // i 的左子节点
  while(j < n) {              //往下循环调整堆结构，保存最小堆
    if(j + 1 < n && heap_[j + 1] < heap_[j]) {  // 令j指向更小的子节点
      ++j;
    }
    if(heap_[i] < heap_[j]) { //如果子节点小于该节点，则不需要调整，直接返回
      break;
    }
    __swap_node(i, j);  //小于则交互，并更新i变量
    i = j;
    j = i * 2 + 1;      //更新j变量，继续往下调整，直到超出范围
  }
  // 如果交换过，那么 i 一定大于 pre_i
  return i > pre_i;
}

// 将堆中两个位置（数组中下标）的数据交互，并更新映射表的映射
int Timer::__swap_node(size_t i, size_t j) {
  if (i >= heap_.size() || j >= heap_.size()) {
    return -1;
  }
  //交互并更新映射表
  std::swap(heap_[i], heap_[j]);
  index_map_[heap_[i].id] = i;
  index_map_[heap_[j].id] = j;
  return 1;
}