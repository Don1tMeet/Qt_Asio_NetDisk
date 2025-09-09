#include "AbstractCon.h"

std::atomic<int> AbstractCon::user_count(0);  // 初始化静态变量

void AbstractCon::setVerify(const bool &status) {
  is_verify_ = status;
}

SSL *AbstractCon::getSSL() const {
  return client_ssl_;
}

int AbstractCon::getSock() const {
  return client_sock_;
}

std::string AbstractCon::getUser() const {
  return std::string(user_info_.user);
}

std::string AbstractCon::getPwd() const {
  return std::string(user_info_.pwd);
}

bool AbstractCon::getIsVip() const {
  return is_vip_;
}

bool AbstractCon::getIsVerify() const {
  return is_verify_;
}

Buffer& AbstractCon::getReadBuffer() {
  return read_buffer_;
}

//发送关闭ssl安全套接字请求
void AbstractCon::closeSSL() {
  if(client_ssl_ != nullptr) {
    int shutdown_code = SSL_shutdown(client_ssl_);
    if(shutdown_code < 0) {
      std::cout << "向客户端:" << user_info_.user << " 发送关闭ssl连接请求出错" << std::endl;
    }
  }
}
