#pragma once

#include "SqlConnPool.h"

// MySql连接池的RAII类，在构造时申请连接对象，析构时自动放回连接池

class SqlConnRAII {
 public:
  SqlConnRAII(std::shared_ptr<sql::Connection>& sql_conn, SqlConnPool *conn_pool);
  ~SqlConnRAII();

 private:
  std::shared_ptr<sql::Connection> sql_conn_;
  SqlConnPool* conn_pool_;
};
