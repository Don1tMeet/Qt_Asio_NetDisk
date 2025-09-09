#include "UpDownCon.h"

UpDownCon::UpDownCon(int sockfd, SSL *ssl) : status_(0) {
  ++user_count;
  client_sock_ = sockfd;
  client_ssl_ = ssl;
}

UpDownCon::~UpDownCon() {
  close();
}

UpDownCon::UpDownCon(const UpDownCon &other){
  status_.store(other.status_.load());    // 原子类型要特殊处理
  copyTask(other.task_);
}

UpDownCon& UpDownCon::operator=(const UpDownCon &other) {
  if (&other == this) { // 防止自赋值
    return *this;
  }

  // 赋值成员变量
  status_.store(other.status_.load());    // 原子类型要特殊处理
  copyTask(other.task_);
  return *this;
}

void UpDownCon::init(const UserInfo &info, const UDtask &task) {
  user_info_ = info;

  copyTask(task);

  is_vip_ = ("1" == std::string(user_info_.is_vip));
}

void UpDownCon::initStatusControl() {
  cv_ = std::make_shared<std::condition_variable>();
  mutex_ = std::make_shared<std::mutex>();
}

void UpDownCon::copyTask(const UDtask &task) {
  task_.task_type.store(task.task_type.load());
  {
    std::lock_guard<std::mutex> lock(task_file_name_mtx_);
    task_.file_name = task.file_name;
  }
  {
    std::lock_guard<std::mutex> lock(task_file_md5_mtx_);
    task_.file_md5 = task.file_md5;
  }

  task_.file_size.store(task.file_size.load());
  task_.handled_size.store(task.handled_size.load());
  task_.parent_dir_id.store(task.parent_dir_id.load());
  task_.file_fd.store(task.file_fd.load());
  task_.file_map.store(task.file_map.load());
}

uint32_t UpDownCon::getTaskTaskType() {
  return task_.task_type.load();
}

std::string UpDownCon::getTaskFileName() {
  std::lock_guard<std::mutex> lock(task_file_name_mtx_);
  return task_.file_name;
}

std::string UpDownCon::getTaskFileMd5() {
  std::lock_guard<std::mutex> lock(task_file_md5_mtx_);
  return task_.file_md5;
}

uint64_t UpDownCon::getTaskFileSize() {
  return task_.file_size.load();
}

uint64_t UpDownCon::getTaskHandledSize() {
  return task_.handled_size.load();
}

uint64_t UpDownCon::getTaskParentDirId() {
  return task_.parent_dir_id.load();
}

int32_t UpDownCon::getTaskFileFd() {
  return task_.file_fd.load();
}

char *UpDownCon::getTaskFileMap() {
  return task_.file_map.load();
}

void UpDownCon::setTaskTaskType(uint32_t type) {
  task_.task_type.store(type);
}

void UpDownCon::setTaskFileName(std::string &name) {
  std::lock_guard<std::mutex> lock(task_file_name_mtx_);
  task_.file_name = name;
}

void UpDownCon::setTaskFileMd5(std::string &md5) {
  std::lock_guard<std::mutex> lock(task_file_md5_mtx_);
  task_.file_md5 = md5;
}

void UpDownCon::setTaskFileSize(uint64_t size) {
  task_.file_size.store(size);
}

void UpDownCon::setTaskHandledSize(uint64_t size) {
  task_.handled_size.store(size);
}

void UpDownCon::setTaskParentDirId(uint64_t id) {
  task_.parent_dir_id.store(id);
}

void UpDownCon::setTaskFileFd(int32_t fd) {
  task_.file_fd.store(fd);
}

void UpDownCon::setTaskFileMap(char *map) {
  task_.file_map.store(map);
}

void UpDownCon::addTaskHandleSize(uint64_t size) {
  task_.handled_size.fetch_add(size);
}

bool UpDownCon::lessEqualAddTaskHandleSize(uint64_t num, uint64_t size) {
  std::lock_guard<std::mutex> lock(task_handled_size_mtx_);
  if (task_.handled_size.load() <= num) {
    addTaskHandleSize(size);
    return true;
  }
  return false;
}

void UpDownCon::close() {
  if (is_close_) {
    return;
  }
  is_close_ = true;
  status_.store(UpDownCon::CLOSE);  // 修改状态，避免其它线程继续处理
  
  if (client_ssl_ != nullptr) {
    int shutdown_result = SSL_shutdown(client_ssl_);
    if (0 == shutdown_result) {
      shutdown_result = SSL_shutdown(client_ssl_);
    }

    if (shutdown_result < 0) {
      std::cerr << "SSL_shutdown failed with error code: " << SSL_get_error(client_ssl_, shutdown_result) << std::endl;
    }
    else {
      std::cout << "客户端关闭ssl成功" << std::endl;
    }
    
    SSL_free(client_ssl_);
    client_ssl_ = nullptr;
  }

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 为什么判断底层socket是否存在来决定是否执行munmap !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if (client_sock_ >= 0) {
    if (::close(client_sock_) < 0) {
      std::cerr << "close() failed with error: " << strerror(errno) << std::endl;
    }
    
    if (munmap(task_.file_map, task_.file_size) != 0) {
      std::cerr << "munmap failed: " << strerror(errno) << std::endl;
    }
    task_.file_map = nullptr;   // 防止重复释放
  }

  if (task_.file_fd >= 0) {
    if (::close(task_.file_fd) < 0) {
      std::cerr << "Failed to close file descriptor: " << strerror(errno) << std::endl;
    }
    task_.file_fd = -1;
  }
  --user_count;
  is_close_ = true;

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // 如果不需要实现断点上传，可以将未完成的任务文件直接删除，避免服务器磁盘空间浪费。如果需要实现，这段代码需要修改，可以使用数据库记录保存客户端上传传输任务状态
  if(task_.handled_size < task_.file_size && task_.task_type == UpDownCon::ConType::PUTTASK){   //任务未完成，且为上传任务
    //文件名为，用户根文件夹+/+文件md5码
    std::string filepath = std::string(ROOTFILEPATH) + "/" + std::string(user_info_.user) + "/" + task_.file_md5;
    if(remove(filepath.c_str()) == 0) {
      std::cout << "Remove file: " << filepath.c_str() << std::endl;
    }
    else {
      std::cout << "Remove file fail: " << filepath.c_str() << std::endl;
    }
  }
  // !!!!!!!!!!!!!!!!!!!!!! 不关闭底层socket吗 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
}

int UpDownCon::getStatus() {
  return status_;
}

void UpDownCon::setStatus(UDStatus status) {
  status_.store(status);
}

bool UpDownCon::wait(std::function<bool()> pred) {
  if (cv_) {
    std::unique_lock<std::mutex> lock(*mutex_.get());
    cv_->wait(lock, pred);  // 等待条件满足
    return true;
  }
  return false;
}


bool UpDownCon::notifyOne() {
  if (cv_) {
    cv_->notify_one();
    return true;
  }
  return false;
}

bool UpDownCon::notifyAll() {
  if (cv_) {
    cv_->notify_all();
    return true;
  }
  return false;
}

bool UpDownCon::cmpExchange(int expected, int desired) {
  // 如果status_是expected，则将status_变为desired，返回true
  // 如果不是，则返回false
  return status_.compare_exchange_strong(expected, desired);
}

std::mutex &UpDownCon::getSendMutex() {
  return send_mutex_;
}
