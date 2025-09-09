#include "EventLoop.h"
#include "Log.h"
#include "ShortTaskTool.h"
#include "LongTaskTool.h"
#include <cassert>
#include <sys/ioctl.h>

EventLoop::EventLoop(std::shared_ptr<WorkQue> work_que, std::shared_ptr<Timer> timer, const int timeout_ms)
  : ep_(std::make_unique<Epoller>(1024)), is_close_(false),
  work_que_(work_que),
  timer_(timer),
  timeout_ms_(timeout_ms)
{

}

EventLoop::~EventLoop() {

}

void EventLoop::loop() {
  while (!is_close_) {
    int event_cnt = ep_->wait();  // 监听事件

    for (int i=0; i<event_cnt; ++i) {
      int fd = ep_->getEventFd(i);
      uint32_t events = ep_->getEvents(i);

      if (events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR)) {   // 如果事件类型是错误、远端关闭挂起、关闭连接等
        assert(client_con_.count(fd) > 0);
        closeCon(client_con_[fd].get());     // 关闭连接
      }
      else if((events & EPOLLIN)) {  // 如果是可读事件
        assert(client_con_.count(fd) > 0);
        handleClientData(client_con_[fd].get());  // 处理客户端数据
      }
      else {
        LOG_ERROR("unexpected events");
      }
    }
  }
}

void EventLoop::close() {
  is_close_.store(true);
}

void EventLoop::addConn(std::shared_ptr<AbstractCon> con, uint32_t events) {
  assert(con);
  int client_fd = con->getSock();
  client_con_[client_fd] = con;

  if(timeout_ms_ > 0) { // 如果设置了超时
    timer_->add(client_fd, timeout_ms_, std::bind(&EventLoop::closeCon, this, client_con_[client_fd].get()));
  }
  ep_->addFd(client_fd, events);
}

// 关闭客户端连接
void EventLoop::closeCon(AbstractCon *client) {
  assert(client);
  LOG_INFO("Client[%d] quit", client->getSock());
  ep_->delFd(client->getSock()); // 删除监听描述符
  client->close();                    // 关闭连接
  client_con_.erase(client->getSock());
}

// 处理客户端发来的数据（只接收并分发原始数据，不做序列化和其它处理）
void EventLoop::handleClientData(AbstractCon *client) {
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
      work_que_->addTask(std::bind(&EventLoop::handleClientTask, this, pdu_buf, client));
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

void EventLoop::handleClientTask(buffer_shared_ptr buf, AbstractCon *client) {
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

// 根据任务代码不同，生成不同的具体工厂类处理短任务
std::shared_ptr<AbstractTool> EventLoop::getTool(PDU &pdu, AbstractCon *con) {
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
std::shared_ptr<AbstractTool> EventLoop::getTool(TranPdu &pdu, AbstractCon *con) {
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

std::shared_ptr<AbstractTool> EventLoop::getTool(TranDataPdu &pdu, AbstractCon *con) {
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

std::shared_ptr<AbstractTool> EventLoop::getTool(TranFinishPdu &pdu, AbstractCon *con) {
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

std::shared_ptr<AbstractTool> EventLoop::getTool(TranControlPdu &pdu, AbstractCon *con) {
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

// 获取底层socket未被处理的数据
int EventLoop::getSocketPending(int sockfd) {
  int pending = 0;
  // 使用FIONREAD获取待读取字节数
  if (ioctl(sockfd, FIONREAD, &pending) == -1) {
    perror("ioctl FIONREAD failed");
    return -1;
  }
  return pending;
}

// 读取ssl和底层socket所有数据，保存到client的read_buffer_中
ssize_t EventLoop::sslReadAll(AbstractCon *client, size_t buf_size) {
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