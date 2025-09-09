#include "SqlConnRAII.h"

SqlConnRAII::SqlConnRAII(std::shared_ptr<sql::Connection>& sql_conn, SqlConnPool *conn_pool) {
  assert(conn_pool != nullptr);
  sql_conn = conn_pool->getConn();
  conn_pool_ = conn_pool;
  sql_conn_ = sql_conn;
}

SqlConnRAII::~SqlConnRAII() {
  if (sql_conn_) {
    conn_pool_->freeConn(sql_conn_);
  }
}