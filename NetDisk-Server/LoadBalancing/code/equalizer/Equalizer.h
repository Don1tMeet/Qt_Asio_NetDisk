#pragma once

#include "Epoller.h"
#include "ServerHeap.h"
#include "Serializer.h"
#include "protocol.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>


class Equalizer{
 public:
  Equalizer(const std::string &ip, const uint16_t port_server, const uint16_t port_client, const std::string &key);
  ~Equalizer();
  void start();

 private:
  bool tcpInit(const std::string &ip, const uint16_t port_server, const uint16_t port_client);
  void handleNewServerConnection();     // 处理新服务器连接
  void handleNewClientConnection();     // 处理新客户端请求  

  void recvServerInfo(int sock);
  void recvServerState(int sock);
  void updateServerState(int sock, size_t new_con_count);
  void addServer(int sock, const size_t count, const std::string &addr, const uint16_t s_port, const uint16_t l_port, const std::string &server_name);
  void delServer(int sock);
  void sendServerInfoToClient(int sock);

  int setFdNonblock(int sock);
  void displayServerInfo();

 private:
  int sock_about_server_{ -1 };   // 与服务器通信的sock
  int sock_about_client_{ -1 };   // 与客户端通信的sock
  
  uint32_t listen_event_ = EPOLLRDHUP | EPOLLHUP | EPOLLERR | EPOLLET;  // 监听事件类型
  
  std::string con_key_;           // 连接负载均衡器的密码
  
  Epoller epoller_;               // epller对象

  ServerHeap server_heap_;        // 保存服务器数据的堆
};