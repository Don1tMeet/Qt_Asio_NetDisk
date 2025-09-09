#include "Log.h"


Log::Log() :
  path_(nullptr), suffix_(nullptr),
  max_lines_(0), line_count_(0), to_day_(0),
  level_(0), is_async_(false), is_open_(false),
  fp_(nullptr)
{
  
}

Log::~Log() {
  // 关闭线程
  // 这可以防止错误地调用 join()，因为如果线程对象已经被联结或分离，再调用 join() 会导致未定义行为。
  if(write_thread_ && write_thread_->joinable()) {
    // deque是线程安全的，因此不需要加锁
    while(!deque_->empty()) {   //等待处理完全部日志后，关闭日志系统，等待线程结束
      deque_->flush();
    };
    deque_->close();
    write_thread_->join();
  }

  // 关闭文件描述符
  {
    std::lock_guard<std::mutex> locker(mtx_);
    if(fp_) {   //保证同步情况下，关闭前写入日志
      flush();
      fclose(fp_);
    }
  }
}

int Log::getLevel() {
  std::lock_guard<std::mutex> locker(mtx_);
  return level_;
}

void Log::setLevel(int level) {
  std::lock_guard<std::mutex> locker(mtx_);
  level_ = level;
}

// 传递参数：level(日志等级，默认为0)，path(日志保存路径)，suffix(日志文件后缀名)，
// maxQueueSize(日志队列最大容量,决定是异步还是同步，为0则同步)初始化日志系统
void Log::init(int level = 0, const char* path, const char* suffix,int max_queue_capacity) {
  is_open_ = true;    //默认开启日志
  level_ = level;     //初始化等级
  if (max_queue_capacity > 0) { //如果队列长度大于0，则异步记录日志
    is_async_ = true;
    if (!deque_) {    //如果队列指针为空，构造一个新队列对象和线程对象，并将其移动到成员变量
      std::unique_ptr<BlockDeque<std::string>> new_deque(new BlockDeque<std::string>);
      deque_ = std::move(new_deque);
      
      std::unique_ptr<std::thread> new_thread(new std::thread(flushLogThread));
      write_thread_ = std::move(new_thread);
    }
  }
  else {    //关闭异步
    is_async_ = false;
  }

  line_count_ = 0;     //当前记录行数为0

  //获取系统当前时间
  time_t timer = time(nullptr);   
  struct tm *sys_time = localtime(&timer);
  struct tm t = *sys_time;

  //设置日志文件路径和后缀名
  path_ = path;
  suffix_ = suffix;

  //创建日记文件名
  char file_name[LOG_NAME_LEN] = {0};
  snprintf(file_name, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
          path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_); //格式化输入字符，将格式化字符数据，写入file_name
  to_day_ = t.tm_mday;     //记录日志文件创建日期

  {
    std::lock_guard<std::mutex> locker(mtx_);     //清空缓冲区，如果fp_已经打开，并且有数据，将其刷新进入系统文件，并关闭
    buff_.retrieveAll();
    if(fp_) { 
      flush();
      fclose(fp_); 
    }

    // 重新打开文件，以追加写的方式打开
    fp_ = fopen(file_name, "a");
    if(fp_ == nullptr) {        //如果文件打开失败，创建目录，并重试0777 是权限位，表示目录的权限设置为读写执行（对所有用户）
      mkdir(path_, 0777);
      fp_ = fopen(file_name, "a");
    } 
    assert(fp_ != nullptr);
  }
}

// 传递日志级别（0:debug、1:info、2:warn、3：error），格式化字符串，将该字符串写入日记文件
void Log::write(int level, const char *format, ...) {
  // 获取当前时间
  struct timeval now = {0, 0};            // 定义时间变量
  gettimeofday(&now, nullptr);            // 获取当前时间
  time_t t_sec = now.tv_sec;              // 转换为时间戳
  struct tm *sys_time = localtime(&t_sec);// 转换为本地时间
  struct tm t = *sys_time;                // 拷贝到新成员，为了保证线程安全，应该这么做

  
  // 检查是否需要切换日志文件，如果时间已经是第二天，或者当前行数已达到最大行数
  {
    std::unique_lock<std::mutex> locker(mtx_);
    if (to_day_ != t.tm_mday || (line_count_ && (line_count_ % MAX_LINES == 0))) {
      locker.unlock();
      
      // 构造新文件
      char new_file[LOG_NAME_LEN];
      char tail[36] = {0};
      snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
      
      locker.lock();  // 上锁，因为要操作类的成员
      if (to_day_ != t.tm_mday) {
        snprintf(new_file, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
        to_day_ = t.tm_mday;
        line_count_ = 0;
      }
      else {
        snprintf(new_file, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (line_count_ / MAX_LINES), suffix_);
      }
      
      // 并将现有缓存写入文件，然后重新创建新文件，并让fp_指针指向新文件
      flush();
      fclose(fp_);
      fp_ = fopen(new_file, "a");
      assert(fp_ != nullptr);
    }
  }
  

  // 将日志记录写入缓冲区
  va_list vaList; // 可变参数列表
  {
    std::unique_lock<std::mutex> locker(mtx_);
    ++line_count_;
    // 将时间写入缓冲区
    int n = snprintf(buff_.beginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
              
    buff_.hasWritten(n);          // 标志已经写入n字节
    __appendLogLevelTitle(level); // 将等级信息写入缓冲区

    // 将格式化字符串写入缓冲区
    va_start(vaList, format);
    int m = vsnprintf(buff_.beginWrite(), buff_.writeAbleBytes(), format, vaList);
    va_end(vaList);

    buff_.hasWritten(m);
    buff_.append("\n\0", 2);    // 将换行符和结束符写入缓冲区


    // 根据是异步还是同步，则异步写入，否则直接同步写入
    if(is_async_ && deque_ && !deque_->full()) {
      deque_->push_back(buff_.retrieveAllToStr());
    }
    else {
      fputs(buff_.beginRead(), fp_);  // fputs将一个以\0结尾的字符串写入文件
    }
    buff_.retrieveAll();    // 清空缓存区
  }
}

void Log::__appendLogLevelTitle(int level) {
  // 治理不需要加锁保护，因为是内部函数，由调用它的函数来确保线程安全
  switch(level) {
  case 0:
    buff_.append("[debug]: ", 9);
    break;
  case 1:
    buff_.append("[info] : ", 9);
    break;
  case 2:
    buff_.append("[warn] : ", 9);
    break;
  case 3:
    buff_.append("[error]: ", 9);
    break;
  default:
    buff_.append("[info] : ", 9);
    break;
  }
}

void Log::flush() {
  // 此函数不能加锁，因为其它函数会调用它，由调用它的函数保证线程安全
  if(is_async_) { 
    deque_->flush(); 
  }
  fflush(fp_);
}

void Log::__asyncWrite() {
  // 循环的从阻塞队列中读取日志数据到str,然后写入fp_
  std::string str = "";
  while(deque_->pop(str)) {
    std::lock_guard<std::mutex> locker(mtx_);
    fputs(str.c_str(), fp_);
  }
}

Log* Log::getInstance() {
  static Log instance;
  return &instance;
}

void Log::flushLogThread() {
  Log::getInstance()->__asyncWrite();
}
