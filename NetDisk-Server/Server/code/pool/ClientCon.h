#pragma once

#include "AbstractCon.h"
#include <mutex>

class ClientCon : public AbstractCon {
 public:
  ClientCon(int sockfd, SSL *ssl);
  ClientCon() = default;
  ~ClientCon() override;

  void init(const UserInfo &info);
  void close() override;

  std::mutex& getSendMutex();

 private:
  std::mutex send_mutex_;
};
