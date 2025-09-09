#include "Equalizer.h"


Equalizer::Equalizer(const std::string &ip, const uint16_t port_server, const uint16_t port_client, const std::string &key)
  : con_key_(key)
{
  if (tcpInit(ip, port_server, port_client)) {
    std::cout << "均衡器已经启动" << "\n"
              << "开始监听地址：" << ip << " 服务器交互端口：" << port_server << " 客户端交互端口："<< port_client << std::endl;
  }
  else {
    std::cout << "均衡器初始化失败" << std::endl;
  }
}

Equalizer::~Equalizer() {
  server_heap_.clear();
}

// 均衡器主循环
void Equalizer::start() {
  while (true) {
    #ifdef DEBUG
      displayServerInfo();  // 仅用于调试
    #endif

    int event_cnt = epoller_.wait();

    for (int i=0; i<event_cnt; ++i) {
      int fd = epoller_.getEventFd(i);
      uint32_t events = epoller_.getEvents(i);

      if(fd == sock_about_server_) {      // 服务端监听套接字
        handleNewServerConnection();
      }
      else if(fd == sock_about_client_) { // 客户端监听套接字
        handleNewClientConnection();  
      }
      else if(events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR)) {  // 如果事件类型是错误、远端关闭挂起、关闭连接等
        delServer(fd); 
      }
      else if(events & EPOLLIN) { // 如果是可读事件，即服务器发送更新信息到达
        recvServerState(fd);     
      }
    }
  }
}

// 初始化均衡器Tcp
bool Equalizer::tcpInit(const std::string &ip, const uint16_t port_server, const uint16_t port_client) {
  // 判断参数是否合法
  if (ip.empty() || port_server < 1024 || port_client < 1024){
    return false;
  }
  
  int ret = 0;
  
  // 创建套接字
  sock_about_server_ = socket(AF_INET, SOCK_STREAM, 0);   // 处理与服务器交互sock
  sock_about_client_ = socket(AF_INET, SOCK_STREAM, 0);   // 处理客户端任务套接字

  // 设置套接字选项
  int on = 1;
  // 设置接受端口重用
  ret = setsockopt(sock_about_server_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  ret = setsockopt(sock_about_client_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  // 设置套接字优雅关闭
  struct linger opt_linger = { {0}, {0} };
  opt_linger.l_onoff = 1;   // 启用 linger
  opt_linger.l_linger = 1;  // 等待 1 秒后关闭
  ret = setsockopt(sock_about_server_, SOL_SOCKET, SO_LINGER, &opt_linger, sizeof(opt_linger));
  ret = setsockopt(sock_about_client_, SOL_SOCKET, SO_LINGER, &opt_linger, sizeof(opt_linger));

  // 绑定套接字
  struct sockaddr_in server_addr;
  memset((char*)&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
  server_addr.sin_port = htons(port_server);
  ret = bind(sock_about_server_, (struct sockaddr*)&server_addr, sizeof(server_addr));

  struct sockaddr_in client_addr = server_addr;
  client_addr.sin_port = htons(port_client);
  ret = bind(sock_about_client_, (struct sockaddr*)&client_addr, sizeof(client_addr));

  // 开始监听
  ret = listen(sock_about_server_, 10);
  ret = listen(sock_about_client_, 10);
  if(-1 == ret) {
    return false;
  }

  // 设置套接字非阻塞
  setFdNonblock(sock_about_server_);
  setFdNonblock(sock_about_client_);

  // 加入到epoll，监听默认事件和可读事件
  epoller_.addFd(sock_about_server_, listen_event_ | EPOLLIN);
  epoller_.addFd(sock_about_client_, listen_event_ | EPOLLIN);

  return true;    
}

// 处理新服务器连接
void Equalizer::handleNewServerConnection() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);

  do {
    int fd = accept(sock_about_server_, (struct sockaddr*)&addr, &len);
    if (fd <= 0) {  // 已经无新连接
      return;
    }
    recvServerInfo(fd);   // 接收服务器信息
  } while(true);
}

// 处理新客户端请求
void Equalizer::handleNewClientConnection() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd = accept(sock_about_client_, (struct sockaddr*)&addr, &len);
    if (fd <= 0) {  // 已经无新连接
      return;
    }
    sendServerInfoToClient(fd); // 发送服务器信息给客户端
  } while(true);
}

// 接受服务器初始连接信息，并添加到堆中
void Equalizer::recvServerInfo(int sock) {
  try {
    ServerInfoPack pack;
    char key[60] = { 0 };
    bool res = false;
    
    // 读取ServerInfo
    auto buf = BufferPool::getInstance().acquire();
    if (read(sock, buf.get(), PROTOCOLHEADER_LEN + SERVERINFOPACK_BODY_LEN) <= 0) {
      std::cerr << "读取服务器信息失败" << std::endl;
      send(sock, (char*)&res, sizeof(res), 0);  // 发送 false 表示错误
      close(sock);
      return;
    }
    
    // 反序列化ServerInfo
    if (!Serializer::deserialize(buf.get(), PROTOCOLHEADER_LEN + SERVERINFOPACK_BODY_LEN, pack)) {
      std::cerr << "反序列化ServerInfo失败" << std::endl;
      send(sock, (char*)&res, sizeof(res), 0);  // 发送 false 表示错误
      close(sock);
      return;
    }

    // 读取key
    // !!!!!!!!!!!!!!!!!!!! key 为什么不放入ServerInfo !!!!!!!!!!!!!!!!!!!!!!!!!
    if (read(sock, key, sizeof(key)) <= 0) {
      std::cerr << "读取密钥失败" << std::endl;
      send(sock, (char*)&res, sizeof(res), 0);  // 发送 false 表示错误
      close(sock);
      return;
    }

    // 检查密钥是否正确
    if (std::string(key) != con_key_) {
      std::cerr << "连接密钥不正确，关闭连接" << std::endl;
      send(sock, (char*)&res, sizeof(res), 0);  // 发送 false 表示密钥错误
      close(sock);
      return;
    }

    // 发送 true 表示成功
    res = true;
    if (send(sock, &res, sizeof(res), 0) < 0) {
      std::cerr << "发送成功响应时出错: " << strerror(errno) << std::endl;
      close(sock);
      return;
    }

    // 将服务器信息添加到堆中
    addServer(sock, pack.cur_con_count, pack.ip, pack.sport, pack.lport, pack.name);

    // 设置非阻塞模式
    setFdNonblock(sock);

    // 将 sock 添加到 epoll 监控的文件描述符中
    epoller_.addFd(sock, listen_event_ | EPOLLIN);

    std::cout << "服务器: " << pack.name << " 连接成功" << std::endl;
  }
  catch (const std::exception &e) {
    bool res = false;
    std::cerr << "接收服务器信息时发生异常: " << e.what() << std::endl;
    send(sock, (char*)&res, sizeof(res), 0);  // 发送 false 表示错误
    close(sock);  // 确保在异常情况下关闭 socket
  }
}

// 接受服务器状态，并更新服务器
void Equalizer::recvServerState(int sock) {
  assert(sock > 0);

  ServerState state;
  auto buf = BufferPool::getInstance().acquire();

  recv(sock, buf.get(), PROTOCOLHEADER_LEN + SERVERSTATE_BODY_LEN, 0);

  if (!Serializer::deserialize(buf.get(), PROTOCOLHEADER_LEN + SERVERSTATE_BODY_LEN, state)) {
    std::cerr << "反序列化ServerState失败" << std::endl;
    return;
  }

  //std::cout<<"当前状态为:"<<state.code<<std::endl;
  //std::cout<<"当前连接数:"<<state.curConCount<<std::endl;

  if (state.code == 0) {  // 如果状态为0，非关闭更新状态
    updateServerState(sock, state.cur_con_count);
  }
  else if(state.code == 1) {  // 关闭状态，从堆中删除该服务器
    delServer(sock);
  }
}

// 更新服务器当前连接数
void Equalizer::updateServerState(int sock, size_t new_con_count) {
  assert(sock > 0);

  // std::cout<<"服务器："<<sock<<" 更新"<<std::endl;
  server_heap_.adjust(sock, new_con_count);
}

// 添加服务器
void Equalizer::addServer(int sock, const size_t count, const std::string &addr, const uint16_t s_port, const uint16_t l_port, const std::string &server_name) {
  assert(sock>0);
  server_heap_.add(sock, addr, s_port, l_port, count, server_name);
}

// 删除服务器
void Equalizer::delServer(int sock) {
  assert(sock > 0);

  std::cout << "服务器" << sock << " 删除" << std::endl;

  server_heap_.pop(sock);
  epoller_.delFd(sock);
  close(sock);
}

// 发送服务器信息给客户端
void Equalizer::sendServerInfoToClient(int sock) {
  try {
    if (sock <= 0) {
      std::cerr << "无效的客户端套接字，无法发送服务器信息" << std::endl;
      return;
    }

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!! 可改为其它调度方式 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // 这里直接获取最少连接的服务器作为该客户的服务器
    ServerNode cur_server;
    if (!server_heap_.getMinServerInfo(cur_server) ) {
      std::cerr << "当前没有服务器" << std::endl;
      return;
    }
    
    ServerInfoPack pack(cur_server.ip, cur_server.s_port, cur_server.l_port, cur_server.server_name);
    auto buf = Serializer::serialize(pack); // 序列化
    // 发送ServerInfo
    ssize_t bytes_sent = send(sock, buf.get(), PROTOCOLHEADER_LEN + SERVERINFOPACK_BODY_LEN, 0);
    if (bytes_sent < 0) {
      std::cerr << "发送服务器信息出错: " << strerror(errno) << std::endl;
      close(sock);
      return;
    }

    close(sock);  // 发送成功后关闭连接
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }
}

// 设置socket非阻塞
int Equalizer::setFdNonblock(int sock) {
  assert(sock > 0);
  return fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
}

// 显示全部服务器的信息
void Equalizer::displayServerInfo() {
  std::vector<ServerNode> &ptr = server_heap_.getVet();
  int ret = system("clear");
  if (-1 == ret) {
    std::cerr << "命令(\"clear\")执行失败：无法创建子进程" << std::endl;
    return;
  }
  std::cout<<"服务器名            IP地址          短任务端口          长任务端口          当前连接数"<<std::endl;
  for(size_t i=0; i<ptr.size(); ++i) {
    std::cout << ptr.at(i).server_name<<"          ";
    std::cout << ptr.at(i).ip<<"          ";
    std::cout << ptr.at(i).s_port<<"                ";
    std::cout << ptr.at(i).l_port<<"                ";
    std::cout << ptr.at(i).cur_con_count<<"         " << std::endl;
  }
}
