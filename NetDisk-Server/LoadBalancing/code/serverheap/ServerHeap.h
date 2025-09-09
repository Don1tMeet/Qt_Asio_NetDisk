#pragma once


#include <unordered_map>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cassert>


struct ServerNode {
  std::string server_name{ "" };// 服务器名
  size_t cur_con_count{ 0 };    // 当前连接数
  std::string ip{ "" };         // ip地址
  uint16_t s_port{ 0 };         // 短任务端口
  uint16_t l_port{ 0 };         // 长任务端口
  int sock{ -1 };               // 与均衡器交互的sock对象
  bool is_close{ false };       // 是否已经关闭

  ServerNode(const size_t &_count, const std::string &_ip, const uint16_t _s_port, const uint16_t _l_port, const int _sock, const std::string &_server_name)
    :server_name(_server_name), cur_con_count(_count), ip(_ip), s_port(_s_port), l_port(_l_port), sock(_sock), is_close(false)
  {

  }

  ServerNode() = default;

  //重载小于运算符，用于堆排序
  bool operator<(const ServerNode &other) const {
    return cur_con_count < other.cur_con_count;
  }
};

class ServerHeap {
 public:
  // 构造函数，初始时候预留64个空间
  ServerHeap();
  ~ServerHeap();

  void adjust(int server_sock, size_t cur_server_con_count);
  void add(int server_sock, const std::string &ip, const uint16_t s_port, const uint16_t l_port, const size_t cur_con_count, const std::string &server_name);
  bool getMinServerInfo(ServerNode &info);
  void pop(int sock);
  void clear();
  std::vector<ServerNode>& getVet();

 private: // 堆的内部操作
  void __del(size_t i);
  void __siftup(size_t i);
  bool __siftdown(size_t pre_i, size_t n);
  void __swapnode(size_t i, size_t j);

 private:
  std::vector<ServerNode> heap_;              // 构建堆的底层容器，存储服务器节点
  std::unordered_map<int, size_t> index_map_; // 服务器结点ID到堆位置的映射，id就是服务器对应的sock
};