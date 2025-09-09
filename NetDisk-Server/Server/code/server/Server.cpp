#include "Server.h"
#include "Epoller.h"
#include "protocol.h"
#include "Timer.h"
#include "Log.h"
#include "WorkQue.h"
#include "MyDB.h"
#include "SRTool.h"
#include "LongTaskTool.h"
#include "ShortTaskTool.h"
#include "ClientCon.h"
#include "UpDownCon.h"
#include "BufferPool.h"
#include "Serializer.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <sys/socket.h>
#include <sys/ioctl.h>
const int sub_reactors_size = 4;

Server::Server (const char *host, int port,int ud_port, const char *sql_user, const char *sql_pwd, const char *db_name, 
int conn_pool_count, int sql_port, int thread_count, int logque_size,int timeout, const char *equalizer_ip,int equalizer_port, const char *equalizer_key, const std::string server_name, bool is_conn_equalizer) {

  (void)sql_port;   // 避免未使用变量的警告

  // 初始化OpenSSL
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  ssl_ctx_ = SSL_CTX_new(TLS_server_method());
  if(!ssl_ctx_)
  {
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  // 加载服务器的整数和密钥
  int load_crt_res = SSL_CTX_use_certificate_file(ssl_ctx_, "server.crt", SSL_FILETYPE_PEM);
  int load_key_res = SSL_CTX_use_PrivateKey_file(ssl_ctx_, "server.key", SSL_FILETYPE_PEM);
  if( load_crt_res <=0 || load_key_res <=0 ){
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }
  SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, NULL);
  
  // 初始化主reactor（epoller）
  epoller_ = std::make_unique<Epoller>();
  listen_event_ = EPOLLRDHUP | EPOLLET;   //初始化监听套接字为对端挂起和边缘触发
  conn_event_ = EPOLLRDHUP | EPOLLHUP |EPOLLERR | EPOLLET;   //初始化客户端事件为对端挂起和边缘触发，和需要重用监视
  std::cout << "Epoller已经初始化" << std::endl;
  
  // 初始化线程池
  work_que_ = std::make_shared<WorkQue>(thread_count);
  std::cout << "任务队列已经初始化" << std::endl;
  
  // 初始化定时器
  timer_ = std::make_shared<Timer>();
  
  // 初始化数据库连接池
  SqlConnPool::getInstance()->init("localhost", sql_user, sql_pwd, db_name, conn_pool_count);   //初始化连接池
  std::cout<<"连接池已经初始化"<<std::endl;
  
  // 初始化日志系统
  Log::getInstance()->init(0, LOGPATH, ".log", logque_size);
  std::cout<<"日记启动"<<std::endl;
  
  // 初始化监听套接字
  timeout_ms_ = timeout;
  initListen(host, port, ud_port);
  std::cout << "tcp服务已经初始化" << std::endl;
  
  // 初始化从reactor
  for (unsigned int i = 0; i != sub_reactors_size; ++i) {
    std::shared_ptr<EventLoop> sub_reactor = std::make_shared<EventLoop>(work_que_, timer_, timeout_ms_);
    sub_reactors_.push_back(sub_reactor);   // 每个子reactor对应一个事件循环
  }
  std::cout << "sub_reactors已经初始化" << std::endl;

  // 连接负载均衡器
  if (is_conn_equalizer && connectEqualizer(equalizer_ip, equalizer_port, host, port, ud_port, server_name, equalizer_key)) {
    equalizer_used_ = true;
    epoller_->addFd(sock_equalizer_, EPOLLRDHUP|EPOLLHUP|EPOLLERR);
    setFdNonblock(sock_equalizer_);
    std::cout<<"已经连接均衡器"<<std::endl;
  }
}

Server::~Server() {
  // 关闭sub_reactors_
  for (int i=0; i!=sub_reactors_size; ++i) {
    sub_reactors_[i]->close();
  }

  // 确保所有任务完成，因为任务需要client_con_
  work_que_->close();

  // 关闭监听套接字
  close(sockfd_);
  close(ud_sockfd_);

  // 关闭数据库连接池
  SqlConnPool::getInstance()->closePool();

  // 关闭负载均衡器
  if (equalizer_used_) {
    sendServerState(1); //发送服务器关闭给均衡器
    close(sock_equalizer_);
  }
}

void Server::start() {
  int time_ms = -1;

  // 开启从事件循环
  for (unsigned int i = 0; i != sub_reactors_.size(); ++i) {  // 启动每个子事件循环
    std::function<void()> sub_pool = std::bind(&EventLoop::loop, sub_reactors_[i].get());
    work_que_->addTask(std::move(sub_pool));
  }

  // 开启主事件循环
  while (true) {
    
    if (timeout_ms_ > 0) {  // 如果设置超时处理
      time_ms = timer_->getNextTick();  // 处理超时客户端
    }
    if (equalizer_used_) {  // 设置间隔10000毫秒和IO事件触发，即10秒发送一次服务器状态信息给均衡器.也可以在连接处理发送，减少性能损耗。
      timerSendServerState(10000);
    }
    // 开始IO复用
    int event_cnt = epoller_->wait(time_ms);

    for (int i=0; i<event_cnt; ++i) {
      int fd = epoller_->getEventFd(i);
      uint32_t events = epoller_->getEvents(i);
      if (fd == sockfd_) {
        handleNewConnection(1);     // 传递为1，处理短连接
      }
      else if (fd == ud_sockfd_) {
        handleNewConnection(2);     // 传递为2，处理长连接
      }
      else if (fd == sock_equalizer_ && (events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR))) { //如果是均衡器关闭
        equalizer_used_ = false;
        std::cout << "均衡器已经关闭" << std::endl;
        epoller_->delFd(fd);
        close(fd);
      }
      else if (events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR)) {   // 如果事件类型是错误、远端关闭挂起、关闭连接等
        assert(client_con_.count(fd) > 0);
        closeCon(client_con_[fd].get());     // 关闭连接
      }
      else {
        LOG_ERROR("unexpected events");
      }
    }
  }
}

// 初始化监听套接字
int Server::initListen(const char* ip, int port, int ud_port) {
  // 创建套接字
  int ret = 0;

  if (port > 65535 || port < 1024 || ud_port > 65535 || ud_port < 1024) {
    std::cerr << "port:" << port << " error";
    return false;
  }

  sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  errCheck(-1 == sockfd_, "create socket error");
  ud_sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  errCheck(-1 == ud_sockfd_, "create socket error");

  // 设置套接字选项
  // 设置端口可重用
  int use = 1;

  ret = setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &use, sizeof(use));
  errCheck(-1 == ret, "setsockopt error");
  ret = setsockopt(ud_sockfd_, SOL_SOCKET, SO_REUSEADDR, &use, sizeof(use));
  errCheck(-1 == ret, "setsockopt error");
  // 设置套接字优雅关闭
  struct linger opt_linger = { {0}, {0} };
  opt_linger.l_onoff = 1;   // 启用linger功能
  opt_linger.l_linger = 1;  // 延迟l_linger秒后关闭（发送未发送的数据）

  ret = setsockopt(sockfd_, SOL_SOCKET, SO_LINGER, &opt_linger, sizeof(opt_linger));
  errCheck(-1 == ret, "setsockopt error");
  ret = setsockopt(ud_sockfd_, SOL_SOCKET, SO_LINGER, &opt_linger, sizeof(opt_linger));
  errCheck(-1 == ret, "setsockopt error");

  // 绑定套接字
  struct sockaddr_in sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_addr.s_addr = inet_addr(ip);
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons(port);

  ret = bind(sockfd_, (sockaddr*)&sock_addr, sizeof(sock_addr));
  errCheck(-1 == ret, "bind error");
  
  struct sockaddr_in ud_sock_addr = sock_addr;
  ud_sock_addr.sin_port = htons(ud_port);

  ret = bind(ud_sockfd_, (sockaddr*)&ud_sock_addr, sizeof(ud_sock_addr));
  errCheck(-1 == ret, "bind error");

  // 开始监听
  ret = listen(sockfd_, 10);
  errCheck(-1 == ret, "listen error");
  ret = listen(ud_sockfd_, 10);
  errCheck(-1 == ret, "listen error");

  setFdNonblock(sockfd_);     // 设置非阻塞
  setFdNonblock(ud_sockfd_);  // 设置非阻塞

  // 添加到epoller，监听客户端连接事件
  epoller_->addFd(sockfd_, listen_event_ | EPOLLIN);    // 将套接字添加到epoll监控,监控默认事件和可读事件
  epoller_->addFd(ud_sockfd_, listen_event_ | EPOLLIN); // 将套接字添加到epoll监控,监控默认事件和可读事件

  return true;
}

// 关闭客户端连接
void Server::closeCon(AbstractCon *client) {
  assert(client);
  LOG_INFO("Client[%d] quit", client->getSock());
  epoller_->delFd(client->getSock()); // 删除监听描述符
  client->close();                    // 关闭连接
  client_con_.erase(client->getSock());
}

// 使用边缘触发，持续处理新连接，直到无新连接
void Server::handleNewConnection(int select) {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  int acceptfd = (select==1 ? sockfd_ : ud_sockfd_);  // 根据传递值，不同决定接受套接字
  do {
    int fd = accept(acceptfd, (struct sockaddr *)&addr, &len);
    if(fd <=0 ) {
      break; // 已经无新连接
    }
    else if(AbstractCon::user_count >= MAX_FD) { // 超出最大连接数
      sendError(fd, "Server busy");
      LOG_WARN("Connect is full");
      return;
    }
    // 进行ssl握手
    SSL *ssl = SSL_new(ssl_ctx_);
    SSL_set_fd(ssl, fd);
    if (SSL_accept(ssl) <= 0) {
      LOG_ERROR("ssl connection fail:%d", fd);
      SSL_free(ssl);
      sendError(fd, "ssl connection fail");
      close(fd);
      continue;
    }
    addClient(fd, ssl, select);
  } while (true);
}

// 处理客户端发来的数据（只接收并分发原始数据，不做序列化和其它处理）
void Server::handleClientData(AbstractCon *client) {
  assert(client);
  
  if(timeout_ms_ > 0) { // 延长超时时间
    timer_->adjust(client->getSock(), timeout_ms_);
  }
  
  // 接收所有数据
  while (true) {
    ssize_t read_bytes = sslReadAll(client, 4096);
    if (read_bytes == -1) {
      // !!!!!!!!!!!!!!!!!!!!! todo !!!!!!!!!!!!!!!!!!!!!!!!!
      std::cout << "read ssl data error" << std::endl;
      break;
    }
    else if (read_bytes == 0) {
      closeCon(client);   // 关闭连接
      std::cout << "client close connection" << std::endl;
      break;
    }
    Buffer& buf = client->getReadBuffer();
    
    // 循环处理数据
    // 如果缓冲区可读数据小于协议头数据（先收到协议头才能确定任务类型和后续接收字节数），直接退出
    while (buf.readAbleBytes() >= PROTOCOLHEADER_LEN) {
      // 获取协议头（不读取）
      ProtocolHeader header;
      Serializer::deserialize(buf.beginRead(), PROTOCOLHEADER_LEN, header);
      // 判断是否能获取完整PDU
      size_t pdu_len = PROTOCOLHEADER_LEN + header.body_len;  // PDU总长度
      if (buf.readAbleBytes() < pdu_len) {
        break;  // 数据还未全部到达，等待下次数据
      }
      // 保存PDU
      assert(BufferPool::getInstance().getBufferSize() >= pdu_len);
      auto pdu_buf = BufferPool::getInstance().acquire();
      memcpy(pdu_buf.get(), buf.beginRead(), pdu_len);
      buf.retrieve(pdu_len);  // 收回（标记以读取）

      // 完整PDU，分发给处理线程
      work_que_->addTask(std::bind(&Server::handleClientTask, this, pdu_buf, client));
    }

    // 检查是否还有数据需要读取
    int ssl_pending = SSL_pending(client->getSSL());
    int socket_pending = getSocketPending(client->getSock());
    if (socket_pending == -1) {
      return;
    }
    // 当SSL内部缓冲区和底层socket都没有数据时，停止读取
    if (ssl_pending == 0 && socket_pending == 0) {
      break;
    }
  }

}

void Server::handleClientTask(buffer_shared_ptr buf, AbstractCon *client) {
  ProtocolHeader header;
  Serializer::deserialize(buf.get(), PROTOCOLHEADER_LEN, header);
  size_t pdu_len = PROTOCOLHEADER_LEN + header.body_len;
  // 根据PDU类型做处理
  switch (header.type) {
    case ProtocolType::PDU_TYPE: {
      // 短任务（PDU）
      PDU pdu;
      bool deserialize_res = Serializer::deserialize(buf.get(), pdu_len, pdu);
      if (!deserialize_res) {
        std::cout << "deserialize PDU failed" << std::endl;
        break;
      }
      // 判断任务类型
      // !!!!!!!!!!!!!!!!!!!!!! 这里会拷贝一次PDU，可优化 !!!!!!!!!!!!!!!!!!!!!!!
      std::shared_ptr<AbstractTool> tool = getTool(pdu, client);
      // 执行任务
      if (tool) {
        tool->doingTask();
      }
      break;
    }
    case ProtocolType::TRANPDU_TYPE: {
      // 长任务（TranPdu）
      TranPdu pdu;
      bool deserialize_res = Serializer::deserialize(buf.get(), pdu_len, pdu);
      if (!deserialize_res) {
        break;
      }
      // 判断任务类型
      std::shared_ptr<AbstractTool> tool = getTool(pdu, client);
      // 执行任务
      if (tool) {
        tool->doingTask();
      }
      break;
    }
    case ProtocolType::TRANDATAPDU_TYPE: {
      TranDataPdu pdu;
      bool deserialize_res = Serializer::deserialize(buf.get(), pdu_len, pdu);
      if (!deserialize_res) {
        std::cout << "deserialize TranDataPdu failed" << std::endl;
        break;
      }
      std::shared_ptr<AbstractTool> tool = getTool(pdu, client);
      // 执行任务
      if (tool) {
        tool->doingTask();
      }
      break;
    }
    case ProtocolType::TRANFINISHPDU_TYPE: {
      TranFinishPdu pdu;
      bool deserialize_res = Serializer::deserialize(buf.get(), pdu_len, pdu);
      if (!deserialize_res) {
        break;
      }
      std::shared_ptr<AbstractTool> tool = getTool(pdu, client);
      // 执行任务
      if (tool) {
        tool->doingTask();
      }
      break;
    }
    case ProtocolType::TRANCONTROLPDU_TYPE: {
      TranControlPdu pdu;
      bool deserialize_res = Serializer::deserialize(buf.get(), pdu_len, pdu);
      if (!deserialize_res) {
        break;
      }
      std::shared_ptr<AbstractTool> tool = getTool(pdu, client);
      // 执行任务
      if (tool) {
        tool->doingTask();
      }
      break;
    }
    default: {
      // 未知类型
      std::cerr << "unknown ProtocolType" << std::endl;
      break;
    }
  }
}

// 接收客户端命令，生成具体工厂分类处理任务
void Server::createFactory(AbstractCon *con) {
  std::shared_ptr<AbstractTool> tool;
  SRTool sr_tool;   //创建一个发收工具
  if (con->client_type == AbstractCon::ConType::SHOTTASK) { //如果是短任务
    PDU pdu;
    sr_tool.recvPDU(con->getSSL(), pdu);
    tool = getTool(pdu, con);
  }
  else if ((con->client_type == AbstractCon::ConType::LONGTASK && con->getIsVerify() == false)) { // 长任务已经首次连接，无法确定是下载还是上传）
    TranPdu pdu;
    sr_tool.recvTranPdu(con->getSSL(), pdu);
    tool = getTool(pdu, con);
  }
  else {  // 长任务已经认证，处于进行中状态
    switch (con->client_type) {
      case AbstractCon::ConType::PUTTASK:           tool = std::make_shared<PutsTool>(con); break;
      case AbstractCon::ConType::GETTASK:           tool = std::make_shared<GetsTool>(con); break;
      case AbstractCon::ConType::GETTASKWAITCHECK:  tool = std::make_shared<GetsTool>(con); break;
    }
  }
  // 执行任务
  if (tool) {
    tool->doingTask();
  }

  // 重新添加监控
  // !!!!!!!!!!!!!!!!!! 疑问：为什么要修改文件描述符的监听事件 !!!!!!!!!!!!!!!!!!!!!!!!
  if (con->client_type == AbstractCon::ConType::GETTASK) {  // 如果是下载任务，需要多监控可写事件
    epoller_->modFd(con->getSock(), conn_event_|EPOLLIN|EPOLLOUT);
  }
  else {
    epoller_->modFd(con->getSock(), conn_event_|EPOLLIN);   // 重新添加监控
  }
}

// 根据任务代码不同，生成不同的具体工厂类处理短任务
std::shared_ptr<AbstractTool> Server::getTool(PDU &pdu, AbstractCon *con) {
  assert(con);

  switch(pdu.code) {
    case Code::SIGNIN: {
      return std::make_shared<LoginTool>(pdu, con);
      break;
    }
    case Code::SIGNUP: {
      return std::make_shared<SignTool>(pdu, con);
      break;
    }
    case Code::CD: {
      return std::make_shared<CdTool>(pdu, con);
      break;
    }
    case Code::MAKEDIR: {
      return std::make_shared<CreateDirTool>(pdu, con);
      break;
    }
    case Code::DELETEFILE: {
      return std::make_shared<DeleteTool>(pdu, con);
      break;
    }
    default:
    break; 
  }
  return std::shared_ptr<AbstractTool>(nullptr);  // 返回空指针
}

// 根据任务代码不同，生成不同的具体工厂类处理长任务
std::shared_ptr<AbstractTool> Server::getTool(TranPdu &pdu, AbstractCon *con) {
  assert(con);

  switch(pdu.tran_pdu_code) {
    case Code::PUTS:{
      con->client_type = AbstractCon::ConType::PUTTASK; // 此时任务类型已经可以确定
      return std::make_shared<PutsTool>(pdu, con);            
    }
    case Code::PUTSCONTINUE: {
      con->client_type = AbstractCon::ConType::PUTTASK; // 此时任务类型已经可以确定
      return std::make_shared<PutsTool>(pdu, con);            
    }
    case Code::GETS: {
      con->client_type = AbstractCon::ConType::GETTASK; // 确定为下载任务
      return std::make_shared<GetsTool>(pdu, con);
    }
    default:
    break;
  }
  return std::shared_ptr<AbstractTool>(nullptr);  // 返回空指针
}

std::shared_ptr<AbstractTool> Server::getTool(TranDataPdu &pdu, AbstractCon *con) {
  assert(con);

  switch (pdu.code) {
    case Code::PUTS_DATA: {
      return std::make_shared<PutsDataTool>(pdu, con);
    }
    case Code::GETS_DATA: {
      return std::make_shared<GetsDataTool>(pdu, con);
    }
    default: {
      break;
    }
  }
  return std::shared_ptr<AbstractTool>();
}

std::shared_ptr<AbstractTool> Server::getTool(TranFinishPdu &pdu, AbstractCon *con) {
  assert(con);

  switch (pdu.code) {
    case Code::PUTS_FINISH: {
      return std::make_shared<PutsFinishTool>(pdu, con);
    }
    case Code::GETS_FINISH: {
      return std::make_shared<GetsFinishTool>(pdu, con);
    }
    default: {
      break;
    }
  }
  return std::shared_ptr<AbstractTool>();
}

std::shared_ptr<AbstractTool> Server::getTool(TranControlPdu &pdu, AbstractCon *con) {
  assert(con);

  switch (pdu.code) {
    case Code::GETS_CONTROL: {
      return std::make_shared<GetsControlTool>(pdu, con);
    }
    default: {
      break;
    }
  }
  return std::shared_ptr<AbstractTool>();
}

// 连接负载均衡器，返回是否连接成功
bool Server::connectEqualizer(const std::string &equalizer_ip, const int &equalizer_port, const std::string &mine_ip, const int mini_sport, const int &mini_lport, const std::string &server_name, const std::string &key) {
  sock_equalizer_ = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_equalizer_ < 0) {
    std::cerr << "Failed to create server socket." << std::endl;
    return false;
  }
  
  // 设置套接字选项，允许地址重用
  int opt = 1;
  if (setsockopt(sock_equalizer_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
    close(sock_equalizer_);
    return false;
  }
  
  // 连接负载均衡器
  struct sockaddr_in equalizer_addr;
  memset((char*)&equalizer_addr, 0, sizeof(equalizer_addr));
  equalizer_addr.sin_family = AF_INET;
  equalizer_addr.sin_addr.s_addr = inet_addr(equalizer_ip.c_str());
  equalizer_addr.sin_port = htons(equalizer_port);
  
  if (connect(sock_equalizer_, (struct sockaddr*)&equalizer_addr, sizeof(equalizer_addr)) < 0) {
    std::cerr << "Failed to connect to the equalizer." << std::endl;
    close(sock_equalizer_);
    return false;
  }
  
  // 发送服务器信息给负载均衡器
  ServerInfoPack pack(mine_ip, mini_sport, mini_lport, server_name);
  auto buf = Serializer::serialize(pack); // 序列化
  if (write(sock_equalizer_, buf.get(), PROTOCOLHEADER_LEN + pack.header.body_len) == -1) {
    close(sock_equalizer_);
    return false;
  }

  // 发送密码给负载均衡器
  char key_buf[60]{ 0 };
  memcpy(key_buf, key.data(), key.size());
  
  if (write(sock_equalizer_, key_buf, sizeof(key_buf)) == -1) {
    close(sock_equalizer_);
    return false;
  }

  // 读取负载均衡器的回复
  bool res;
  res = read(sock_equalizer_, (char*)&res, sizeof(res));
  return res;
}

// 向均衡器发送状态信息
void Server::sendServerState(int state) {
  ServerState cur;
  cur.header.type = ProtocolType::SERVERSTATE_TYPE;
  cur.header.body_len = SERVERSTATE_BODY_LEN;
  cur.code = state;
  cur.cur_con_count = AbstractCon::user_count;     //当前用户数

  auto buf = Serializer::serialize(cur);  // 序列化
  send(sock_equalizer_, buf.get(), PROTOCOLHEADER_LEN + cur.header.body_len, 0);
}

// 间隔millisecond毫秒向负载均衡器发送状态信息
void Server::timerSendServerState(int millisecond) {
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 后续可使用定时器 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // 最后发送的时间
  static auto last_time = std::chrono::high_resolution_clock::now();
  // 获取当前时间
  auto current_time = std::chrono::high_resolution_clock::now();
  // 计算时间差
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time);

  // 检查是否超过设定的时间间隔
  if (duration.count() > millisecond) {
    sendServerState(0);       // 向均衡器发送信息
    last_time = current_time; // 更新最后发送时间
  }
}

// 设置为非阻塞
int Server::setFdNonblock(int fd) {
  assert(fd > 0);
  return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

void Server::sendError(int fd, const char *info) {
  ssize_t bytes = write(fd, info, sizeof(info));
  if (bytes == -1) {
    LOG_ERROR("send error failed");
  }
}

//添加新客户端
void Server::addClient(int client_fd, SSL *ssl, int select) {
  assert(client_fd > 0);
  assert(ssl != nullptr);
  
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!! 这里主从reactor都会持有这个连接，后续修改 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if (select == 1) {    // 如果是短任务连接
    client_con_[client_fd] = std::make_shared<ClientCon>(client_fd, ssl);   // 生成一个连接对象，添加进哈希映射
    client_con_[client_fd]->client_type = AbstractCon::ConType::SHOTTASK;   // 短任务
  }
  else {                // 如果是长任务连接
    client_con_[client_fd] = std::make_shared<UpDownCon>(client_fd, ssl);
    client_con_[client_fd]->client_type = AbstractCon::ConType::LONGTASK;
  }

  setFdNonblock(client_fd);   // 设置套接字非阻塞
  
  // 调度策略，随机
  uint64_t random = client_fd % sub_reactors_.size();
  sub_reactors_[random]->addConn(client_con_[client_fd], EPOLLIN | conn_event_);

  LOG_INFO("client[%d] in", client_fd);             // 记录日记
}

// 获取底层socket未被处理的数据
int Server::getSocketPending(int sockfd) {
  int pending = 0;
  // 使用FIONREAD获取待读取字节数
  if (ioctl(sockfd, FIONREAD, &pending) == -1) {
    perror("ioctl FIONREAD failed");
    return -1;
  }
  return pending;
}

// 读取ssl和底层socket所有数据，保存到client的read_buffer_中
ssize_t Server::sslReadAll(AbstractCon *client, size_t buf_size) {
  SSL* ssl = client->getSSL();
  Buffer& buf = client->getReadBuffer();
  if (!ssl || buf_size == 0) {
    std::cerr << "Invalid parameters" << std::endl;
    return -1;
  }

  int sockfd = SSL_get_fd(ssl); // 获取底层socket
  size_t total_read = 0;        // 总共读取的字节
  int continue_reading = 1;     // 是否继续读

  while (continue_reading && total_read < buf_size) {
    buf.ensureWriteAble(buf_size - total_read); // 确保能够读取字节
    // 尝试读取数据
    size_t ret = SSL_read(ssl, buf.beginWrite(), buf_size - total_read);
    
    if (ret > 0) {  // 读取成功
      buf.hasWritten(ret);  // 标记写了ret字节
      total_read += ret;
    }
    else if (ret == 0) {  // 对方关闭连接
      return ret;
    }
    else {  // 处理错误
      int ssl_err = SSL_get_error(ssl, ret);
      switch (ssl_err) {
        case SSL_ERROR_WANT_READ: {
          // 需要继续读取，但先检查缓冲区状态
          std::cout << "continue read ssl" << std::endl;
          break;
        }
        case SSL_ERROR_WANT_WRITE: {
          // 需要先发送数据（如SSL握手/重协商）
          std::cerr << "Need to write data first, breaking read loop" << std::endl;
          continue_reading = 0;
          break;
        }
        default: {
          // 致命错误
          std::cout << "SSL read error: " << ssl_err << std::endl;
          ERR_print_errors_fp(stdout);
          return -1;
        }
      }
    }

    // 检查是否还有数据需要读取
    int ssl_pending = SSL_pending(ssl);
    int socket_pending = getSocketPending(sockfd);

    if (socket_pending < 0) { // 底层检查出错
      std::cout << "socket error" << std::endl;
      return -1;
    }

    // 当SSL内部缓冲区和底层socket都没有数据时，停止读取
    if (ssl_pending == 0 && socket_pending == 0) {
      continue_reading = 0;
    }
  }

  return total_read;
}
