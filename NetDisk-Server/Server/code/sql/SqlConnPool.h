#pragma once

#include <iostream>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <assert.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <cppconn/prepared_statement.h>
#include "protocol.h"


class SqlConnPool {
 public:
  static SqlConnPool* getInstance();

  // 静止拷贝和移动
  SqlConnPool(const SqlConnPool&) = delete;
  SqlConnPool(SqlConnPool&&) = delete;
  SqlConnPool& operator=(const SqlConnPool&) = delete;
  SqlConnPool& operator=(SqlConnPool&&) = delete;

  // 初始化连接池
  void init(const std::string& host, const std::string& user, const std::string& password,
            const std::string& database, int conn_size = 10);

  std::shared_ptr<sql::Connection> getConn();   // 获取一个连接
  void freeConn(std::shared_ptr<sql::Connection> conn); // 释放连接
  int getFreeConnCount();   // 获取当前可用连接数
  void closePool();         // 关闭连接池

 private:
  SqlConnPool() = default;
  ~SqlConnPool();

  int max_conn_;    // 最大连接数
  std::queue<std::shared_ptr<sql::Connection>> conn_queue_; // 连接队列
  std::mutex mtx_;  // 互斥锁
  std::condition_variable cv_;  // 条件变量，用于线程间通信
  sql::Driver* driver_;   // MySql驱动
};
