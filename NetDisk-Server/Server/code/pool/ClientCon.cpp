#include "ClientCon.h"

ClientCon::ClientCon(int sockfd, SSL *ssl) {
  client_sock_ = sockfd;
  client_ssl_ = ssl;
  ++user_count;
}

ClientCon::~ClientCon() {
  close();
}

void  ClientCon::init(const UserInfo &info) {
  user_info_ = info;
  is_vip_ = (std::string(user_info_.is_vip) == "1");
}

void ClientCon::close() {
  if (is_close_) {
    return;
  }
  // 关闭ssl
  if (client_ssl_ != nullptr) {
    int shutdown_result = SSL_shutdown(client_ssl_);
    if (shutdown_result == 0) {   // 等待客户端接收关闭ssl连接
      shutdown_result = SSL_shutdown(client_ssl_);
    }

    if (shutdown_result < 0) {
      std::cerr << "SSL_shutdown failed with error code: " << SSL_get_error(client_ssl_, shutdown_result) << std::endl;
    }
    SSL_free(client_ssl_);
    client_ssl_ = nullptr; 
  }
  // 关闭socket
  if (client_sock_ >= 0) {
    if (::close(client_sock_) < 0) {
      std::cerr << "close() failed with error: " << strerror(errno) << std::endl;
    }
  }
  --user_count;    //减去客户端连接数1
  is_close_ = true;
}

std::mutex &ClientCon::getSendMutex() {
  return send_mutex_;
}