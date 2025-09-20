#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <openssl/ossl_typ.h>
#include "EventLoop.h"
#include "Serializer.h"
#include "protocol.h"

class WorkQue;
class Timer;
class Epoller;
class AbstractCon;
class AbstractTool;

class Server {
 public:
  // 禁止拷贝
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(Server&&) = delete;

  Server (const char *host, int port,int ud_port, const char *sql_user, const char *sql_pwd, const char *db_name, 
int conn_pool_count, int sql_port, int thread_count, int logque_size,int timeout, const char *equalizer_ip,int equalizer_port, const char *equalizer_key, const std::string server_name, bool is_conn_equalizer);
  ~Server();
  void start();

 private: // 主要功能
  int initListen(const char* ip, int port, int ud_port);
  void closeCon(AbstractCon *client);
  void handleNewConnection(int select);
  void handleClientData(AbstractCon *client); // 处理客户端发送过来的数据
  void handleClientTask(buffer_shared_ptr buf, AbstractCon *client);

 private: // 辅助函数
  bool connectEqualizer(const std::string &equalizer_ip, const int &equalizer_port, const std::string &mine_ip, const int mini_sport, const int &mini_lport, const std::string &server_name, const std::string &key);
  void sendServerState(int state);
  void timerSendServerState(int millisecond);
  int setFdNonblock(int fd);
  void sendError(int fd, const char *info);
  void addClient(int client_fd, SSL *ssl, int select);
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
  std::shared_ptr<EventLoop> main_reactor_; // 当前不使用
  std::vector<std::shared_ptr<EventLoop>> sub_reactors_;

  // 代替main_reactor_
  std::shared_ptr<WorkQue> work_que_;       //线程池，工作队列，用于添加任务
  std::shared_ptr<Timer> timer_;            //用于处理定时器超时，断开超时无操作连接
  std::unique_ptr<Epoller> epoller_;        //epoll字柄
  std::unordered_map<int,std::shared_ptr<AbstractCon>> client_con_;    //套接字与用户连接对象的映射

  int sockfd_ = -1;                   //服务端监听sock,处理新连接，处理短任务。如登陆，注册
  int timeout_ms_ = -1;               //超时时间，单位毫秒
  static const int MAX_FD = 65536;    //最大文件描述符数
  
  SSL_CTX *ssl_ctx_ = nullptr;    //安全套接字
  uint32_t listen_event_ = 0;     //监听套接字默认监控事件：EPOLLRDHUP（对端关闭） EPOLLIN(可读事件) EPOLLET（边缘触发）
  uint32_t conn_event_ = 0;       //客户端连接默认监控事件：//连接默认监控事件EPOLLONESHOT（避免多线程，下次需要从新设置监听） | EPOLLRDHUP

  //处理长耗时操作
  int ud_sockfd_ = -1;         //处理上传和下载任务的sockfd描述符

  int sock_equalizer_ = -1;     //服务器与负载均衡器交互sock
  bool equalizer_used_ = false; //负载均衡器是否使用
};
