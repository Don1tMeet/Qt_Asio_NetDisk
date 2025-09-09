#pragma once

#include "protocol.h"
#include "Buffer.h"
#include <atomic>
#include <assert.h>


// !!!!!!!!!!!!!!!!!!!!!!!!!!!! 不是线程安全的且可能有多个线程同时修改成员 !!!!!!!!!!!!!!!!!!!!!!!!!!!!
class AbstractCon {
 public:
  static std::atomic<int> user_count;   // 原子类型的连接总数，保存整个服务器连接总数

  AbstractCon() = default;
  virtual ~AbstractCon() = default;
  virtual void close() = 0;   // 纯虚函数

  void setVerify(const bool &status);
  SSL *getSSL() const;
  int getSock() const;
  std::string getUser() const;
  std::string getPwd() const;
  bool getIsVip() const;
  bool getIsVerify() const;
  Buffer& getReadBuffer();
  void closeSSL();

  enum ConType{
    SHOTTASK=0,       // 短任务
    LONGTASK,         // 长任务
    PUTTASK,          // 上传
    GETTASK,          // 下载
    GETTASKWAITCHECK  // 下载完毕，等待客户端检查
  };

  int client_type = 0;

 protected:
  SSL *client_ssl_ = nullptr;     // 客户端ssl套接字
  int client_sock_ = -1;          // 客户端sock
  UserInfo user_info_;            // 用户信息体
  bool is_verify_ = false;        // 客户端是否已经进行登陆认证，用来确定能否进行其它操作
  bool is_close_ = false;         // 客户端是否已经关闭
  bool is_vip_ = false;           // 是否vip用户

  Buffer read_buffer_;            // 读缓冲区
};
