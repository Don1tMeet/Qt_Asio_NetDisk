#include "ServerHeap.h"

ServerHeap::ServerHeap() {
  heap_.reserve(64);  // 预先分配64大小
}

ServerHeap::~ServerHeap() {
  clear();
}

// 输入与服务器关联的socket，修改它的连接数，并调整位置
void ServerHeap::adjust(int server_sock, size_t cur_server_con_count) {
  assert(!heap_.empty() && index_map_.count(server_sock) > 0);

  heap_[index_map_[server_sock]].cur_con_count = cur_server_con_count;

  // 调整在堆中的位置
  if (!__siftdown(index_map_[server_sock], heap_.size())) {
    __siftup(index_map_[server_sock]);  // 如果无法下移，则可能需要上移
  }
}

// 输入服务器socket和其它信息，将它加入到堆中
void ServerHeap::add(int server_sock, const std::string &ip, const uint16_t s_port, const uint16_t l_port, const size_t cur_con_count, const std::string &server_name) {
  assert(server_sock >= 0);

  if(index_map_.count(server_sock) == 0) {  // 不存在该节点，插入
    // 新节点：堆尾插入，调整堆
    size_t i = heap_.size();
    index_map_[server_sock] = i;
    heap_.push_back( {cur_con_count, ip, s_port, l_port, server_sock, server_name} );
    __siftup(i);
  }
  else {  // 存在，更新节点
    size_t i = index_map_[server_sock];
    heap_[i].cur_con_count = cur_con_count;
    heap_[i].ip = ip;
    heap_[i].s_port = s_port;
    heap_[i].l_port = l_port;
    heap_[i].sock = server_sock;
    heap_[i].server_name = server_name;
    if(!__siftdown(i, heap_.size())) {  // 调整位置
      __siftup(i);
    }
  }
}

// 获取连接数最小服务器信息，保存到info
bool ServerHeap::getMinServerInfo(ServerNode &info) {
  if(heap_.size() > 0) {
    info = heap_[0];
    return true;
  }
  return false;
}

// 删除指定socket的节点
void ServerHeap::pop(int sock) {
  if(index_map_.count(sock) == 0) { // 确保 sock 存在于映射中
    return;
  }

  size_t i = index_map_[sock];
  __del(i);
}

// 清空堆和map
void ServerHeap::clear() {
  index_map_.clear();
  heap_.clear();
}

// 获取堆的原始数据
std::vector<ServerNode>& ServerHeap::getVet() {
  return heap_;
}

// 堆的内部操作：输入节点下标，从堆和map中删除，并调整堆
void ServerHeap::__del(size_t i) {
  assert(!heap_.empty() && i < heap_.size());

  size_t n = heap_.size() - 1;    // 最大下标
  if(i < n) {
    __swapnode(i, n);   // 将其交换到最后节点
    // 交换之后需要重新调整处于i（原来的n）位置的节点
    if(!__siftdown(i, n)) {
      __siftup(i);
    }
  }

  // 在堆和map中删除
  index_map_.erase(heap_.back().sock);
  heap_.pop_back();
}

// 输入下标，根据它的连接数，来往上移动
void ServerHeap::__siftup(size_t i) {
  assert(i < heap_.size());

  while(i > 0) {  // 循环直到根节点，i == 0 时停止
    size_t j = (i - 1) / 2;     // 父节点
    if(heap_[j] < heap_[i]) {   // 父节点已小于当前节点，结束调整
      break;
    }
    __swapnode(i, j);     // 交换
    i = j;                // 更新当前节点索引
  }
}

// 输入节点和堆长度，向下调整堆
bool ServerHeap::__siftdown(size_t pre_i, size_t n) {
  assert(pre_i < n && n <= heap_.size());

  // 由于要判断是否调整过，因此要保存调整之前的节点pre_i
  size_t i = pre_i;           // 保存索引
  size_t j = i * 2 + 1;       // 获取 i 左子节点
  while(j < n) {  // 往下循环调整堆，保存最小堆
    if(j + 1 < n && heap_[j + 1] < heap_[j]) {
      ++j;  // 选择左右子节点中较小的一个交换
    }
    if(heap_[i] < heap_[j]) { // 当前已满足小根堆，结束
      break;
    }
    __swapnode(i, j);

    i = j;
    j = i * 2 + 1;
  }

  return i > pre_i; // 如果交换了i一定会增大
}

// 交换两个节点的数据，并更新map
void ServerHeap::__swapnode(size_t i, size_t j) {
  assert(i < heap_.size() && j < heap_.size());

  std::swap(heap_[i], heap_[j]);  // 交换数据
  // 更新map
  index_map_[heap_[i].sock] = i;
  index_map_[heap_[j].sock] = j;
}
