#include "SqlConnPool.h"


SqlConnPool* SqlConnPool::getInstance() {
  static SqlConnPool instance;
  return &instance;
}

// 初始化连接池
void SqlConnPool::init(const std::string& host, const std::string& user, const std::string& password,
                       const std::string& database, int conn_size) {
  assert(conn_size > 0);

  std::lock_guard<std::mutex> lock(mtx_);
  
  driver_ = get_driver_instance();  // 获取MySql驱动
  for (int i=0; i<conn_size; ++i) {
    try {
      std::shared_ptr<sql::Connection> conn(driver_->connect(host, user, password));
      conn->setSchema(database);  // 设置数据库
      conn_queue_.push(conn);     // 添加到队列
    }
    catch (sql::SQLException& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      throw std::runtime_error("Failed to create MySQL connection.");
    }
  }

  max_conn_ = conn_size;
}

// 获取连接
std::shared_ptr<sql::Connection> SqlConnPool::getConn() {
  std::unique_lock<std::mutex> lock(mtx_);

  // 如果连接池为空，等待
  cv_.wait(lock, [this]() {return !conn_queue_.empty(); });

  // 从队列中取出连接
  auto conn = conn_queue_.front();
  conn_queue_.pop();

  return conn;
}

// 释放连接
void SqlConnPool::freeConn(std::shared_ptr<sql::Connection> conn) {
  std::lock_guard<std::mutex> lock(mtx_);

  conn_queue_.push(conn); // 将连接放回队列
  cv_.notify_one();   // 通知等待线程
}

// 获取当前可用连接数
int SqlConnPool::getFreeConnCount() {
  std::lock_guard<std::mutex> lock(mtx_);
  return conn_queue_.size();
}

// 关闭连接池
void SqlConnPool::closePool() {
  std::lock_guard<std::mutex> lock(mtx_);

  // 清空队列
  while(!conn_queue_.empty()) {
    conn_queue_.pop();
  }
}

// 析构函数
SqlConnPool::~SqlConnPool() {
  closePool();
}