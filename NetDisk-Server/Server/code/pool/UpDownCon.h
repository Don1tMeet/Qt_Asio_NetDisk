#pragma once


#include "AbstractCon.h"
#include <mutex>
#include <memory>
#include <condition_variable>
#include <atomic>

struct UDtask {
  std::atomic<uint32_t> task_type{ AbstractCon::ConType::LONGTASK };  // 任务类型，默认为长任务，后面根据需要改为下载或上传
  std::string file_name;                    // 文件名
  std::string file_md5;                     // 文件存储在磁盘中的
  std::atomic<uint64_t> file_size{ 0 };     // 文件总大小（字节）
  std::atomic<uint64_t> handled_size{ 0 };  // 已处理大小（字节）
  std::atomic<uint64_t> parent_dir_id{ 0 }; // 保存在哪个文件夹下，默认为0（根目录）
  std::atomic<int32_t> file_fd{ -1 };       // 文件套接字
  std::atomic<char*> file_map{ nullptr };   // 文件内存映射
  
  UDtask() = default;
  // 重载拷贝函数
  UDtask(const UDtask& other) {
    task_type.store(other.task_type.load());
    file_name = other.file_name;
    file_md5 = other.file_md5;
    file_size.store(other.file_size.load());
    handled_size.store(other.handled_size.load());
    parent_dir_id.store(other.parent_dir_id.load());
    file_fd.store(other.file_fd.load());
    file_map.store(other.file_map.load());
  }
  UDtask& operator=(const UDtask& other) {
    task_type.store(other.task_type.load());
    file_name = other.file_name;
    file_md5 = other.file_md5;
    file_size.store(other.file_size.load());
    handled_size.store(other.handled_size.load());
    parent_dir_id.store(other.parent_dir_id.load());
    file_fd.store(other.file_fd.load());
    file_map.store(other.file_map.load());

    return *this;
  }
};


// 处理长任务连接的类（上传，下载）（线程安全）
class UpDownCon : public AbstractCon {
 public:
  enum UDStatus { // 上传下载状态
    START = 0,    // 开始
    DOING,        // 上传下载中
    PAUSE,        // 暂停
    FIN,          // 完成
    CLOSE         // 关闭
  };


  UpDownCon(int sockfd, SSL *ssl);
  UpDownCon() = default;
  ~UpDownCon() override;

  // 因为原子类型的拷贝需要特殊处理，所以重载拷贝相关函数
  UpDownCon(const UpDownCon &other);   // 重载拷贝构造函数
  UpDownCon& operator= (const UpDownCon &other);  // 重载赋值运算符

  // 对UDtask的操作，因为考虑到多线程和封装性的问题，task因该由UpDownCon本身来修改
  void init(const UserInfo &info, const UDtask &task);
  void initStatusControl();
  void copyTask(const UDtask &task);

  uint32_t getTaskTaskType();
  std::string getTaskFileName();
  std::string getTaskFileMd5();
  uint64_t getTaskFileSize();
  uint64_t getTaskHandledSize();
  uint64_t getTaskParentDirId();
  int32_t getTaskFileFd();
  char* getTaskFileMap();

  void setTaskTaskType(uint32_t type);
  void setTaskFileName(std::string& name);
  void setTaskFileMd5(std::string& md5);
  void setTaskFileSize(uint64_t size);
  void setTaskHandledSize(uint64_t size);
  void setTaskParentDirId(uint64_t id);
  void setTaskFileFd(int32_t fd);
  void setTaskFileMap(char* map);
  
  // task_ 的 handled_size 相关操作
  void addTaskHandleSize(uint64_t size);  // task_.handle_size += size
  // 小于等于 num 执行 task_handled_size += size，该操作为原子操作
  bool lessEqualAddTaskHandleSize(uint64_t num, uint64_t size);

  void close() override;

  int getStatus();
  void setStatus(UDStatus status);
  bool wait(std::function<bool()> pred);
  bool notifyOne();
  bool notifyAll();
  bool cmpExchange(int expected, int desired);

  std::mutex& getSendMutex();

 private:
  // 控制运行状态 
  std::atomic<int> status_{ 0 };  // 原子类型，因为一个上传下载任务可能被多个线程修改
  std::shared_ptr<std::condition_variable> cv_{ nullptr };  // 控制status_的条件变量
  std::shared_ptr<std::mutex> mutex_{ nullptr };
  
  
  UDtask task_;
  std::mutex task_file_name_mtx_;     // 保护 task_ 的 file_name 的互斥锁
  std::mutex task_file_md5_mtx_;      // 保护 task_ 的 file_md5 的互斥锁
  std::mutex task_handled_size_mtx_;  // 保护 task_ 的 handled_size 的互斥锁

  std::mutex send_mutex_;             // 发送锁，保证发送回复的原子性

};
