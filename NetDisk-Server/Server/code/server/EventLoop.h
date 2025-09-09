#pragma once

#include "Epoller.h"
#include "AbstractCon.h"
#include "BufferPool.h"
#include "Serializer.h"
#include "AbstractTool.h"
#include "WorkQue.h"
#include "Timer.h"
#include <memory>
#include <unordered_map>
#include <atomic>


class EventLoop {
 public:
  EventLoop(std::shared_ptr<WorkQue> work_que, std::shared_ptr<Timer> timer, const int timeout_ms);
  ~EventLoop();

  void loop();
  void close();
  void addConn(std::shared_ptr<AbstractCon> con, uint32_t events);

 private:
  void closeCon(AbstractCon* client);
  void handleClientData(AbstractCon *client);
  void handleClientTask(buffer_shared_ptr buf, AbstractCon *client);

  std::shared_ptr<AbstractTool> getTool(PDU &pdu, AbstractCon *con);
  std::shared_ptr<AbstractTool> getTool(TranPdu &pdu, AbstractCon *con);
  std::shared_ptr<AbstractTool> getTool(TranDataPdu &pdu, AbstractCon *con);
  std::shared_ptr<AbstractTool> getTool(TranFinishPdu &pdu, AbstractCon *con);
  std::shared_ptr<AbstractTool> getTool(TranControlPdu &pdu, AbstractCon *con);

  // 获取底层socket未被处理的数据
  int getSocketPending(int sockfd);
  // 读取ssl和底层socket所有数据
  ssize_t sslReadAll(AbstractCon *client, size_t buf_size);

 private:
  std::unique_ptr<Epoller> ep_;
  std::atomic<bool> is_close_{ false };
  std::shared_ptr<WorkQue> work_que_;     // 线程池，工作队列，用于添加任务
  std::shared_ptr<Timer> timer_;          // 用于处理定时器超时，断开超时无操作连接
  int timeout_ms_{ -1 };                  // 超时时间，单位毫秒

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!! 后续改为AbstractCon绑定对应的EventLoop !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  std::unordered_map<int,std::shared_ptr<AbstractCon>> client_con_;    //套接字与用户连接对象的映射
};
