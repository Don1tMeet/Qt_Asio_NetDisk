#include "LongTaskTool.h"
#include "Log.h"

//*******************************************上传任务*******************************************//
PutsTool::PutsTool(AbstractCon *conn) : conn_parent_(conn) {

}

PutsTool::PutsTool(const TranPdu &pdu, AbstractCon *conn) : pdu_(pdu), conn_parent_(conn) {

}

// 上传
int PutsTool::doingTask() {
  UpDownCon *conn = dynamic_cast<UpDownCon*>(conn_parent_); //转换成子类对象
  if(conn->getStatus() == UpDownCon::UDStatus::START) {   // 如果刚刚开始，没有进行连接和认证
    firstCheck(conn);
  }
  if(conn->getStatus() == UpDownCon::CLOSE) {  //关闭状态
    // conn->close();
  }
  return 0;
}

// 初始认证
bool PutsTool::firstCheck(UpDownCon *conn) {
  MyDB db;
  PDURespond respond;
  respond.header.type = ProtocolType::PDURESPOND_TYPE;
  respond.header.body_len = PDURESPOND_BODY_BASE_LEN;
  respond.code = Code::PUTS;
  respond.msg_amount = 0;
  respond.msg_len = 0;
  UserInfo info;

  bool sql_res = db.getUserInfo(pdu_.user, pdu_.pwd, info); // 获取用户信息

  if(sql_res && std::string(info.cipher) != "") {   //客户端发送过来的用户名和密码通过认证
    UDtask task = createTask(respond, db);  // 创建任务
    if(respond.status == Status::SUCCESS || respond.status == Status::PUT_CONTINUE_FAILED || respond.status==Status::PUT_QUICK) {
      conn->init(info, task);     //保存客户信息，初始化连接类
      conn->setVerify(true);      //设置客户端已经通过认证

      if(respond.status == Status::PUT_QUICK) { //如果可以秒传，直接完成
        conn->setStatus(UpDownCon::UDStatus::FIN);
        std::cout << "upload file quick" << std::endl;
      }
      else {
        conn->setStatus(UpDownCon::UDStatus::DOING);
        std::cout << "upload file start" << std::endl;
      }
    }
  }
  else {  // 找不到用户，或用户没有验证身份
    respond.status = Status::FAILED;
  }
  
  // 发送回复
  {
    // 理论上不会有多个线程同时调用，但任然加锁
    std::lock_guard<std::mutex> lock(conn->getSendMutex());
    sr_tool_.sendPDURespond(conn->getSSL(), respond);   //将结果发回客户端
  }
  if(respond.status == Status::FAILED || respond.status == Status::NO_CAPACITY) { //出错改成重新认证
    conn->client_type = AbstractCon::LONGTASK;
    conn->setVerify(false);
  }

  return true;
}

// 创建任务
UDtask PutsTool::createTask(PDURespond &respond, MyDB &db) {
  UDtask task;
  task.task_type = UpDownCon::ConType::PUTTASK;   // 任务类型为上传
  task.file_name = pdu_.file_name;                // 任务文件名
  task.file_md5 = std::string(pdu_.file_md5);     // 文件MD5码,加上用户根文件夹，用户名就是根文件夹名，保证用户名唯一，所以文件夹唯一
  task.file_size = pdu_.file_size;                // 文件总大小
  task.parent_dir_id = pdu_.parent_dir_id;        // 父文件夹ID

  std::cout << "upload file info:\n" 
            << "file_name: " << task.file_name << '\n'
            << "file_md5: " << task.file_md5 << '\n'
            << "file_size: " << task.file_size << std::endl;

  std::string suffix = getSuffix(pdu_.file_name); // 后缀名检查
  if(suffix == "d") {   // 文件夹，不合法
    std::cout << "PutsTool::createTask(RespondPack&, MyDB&): " << "can't upload dir" << std::endl;
    respond.status = Status::FAILED;
    return task;
  }

  if(!db.getIsEnoughSpace(pdu_.user, pdu_.pwd, pdu_.file_size)) { //如果空间不足
    respond.status = Status::NO_CAPACITY;
    return task;
  }

  if(db.getFileExist(pdu_.user, pdu_.file_md5)) { //如果文件已经存在，支持秒传
    respond.status = Status::PUT_QUICK;
    task.handled_size.store(task.file_size.load()); //修改已经处理大小为总大小，否则会导致文件被删除
    return task;    //返回即可，后面无需操作
  }

  respond.status = Status::SUCCESS;  //暂时默认为OK
  struct stat is_exist;
  std::string open_file_name = std::string(ROOTFILEPATH) + "/" + std::string(pdu_.user) + "/" + std::string(pdu_.file_md5);
  std::cout << "PutsTool::createTask(RespondPack&, MyDB&): " << "upload file path: " << open_file_name << std::endl;
  
  // 如果客户端要求断点续传
  if(pdu_.tran_pdu_code == Code::PUTSCONTINUE) {
    if (stat(open_file_name.c_str(), &is_exist) == 0) { // 如果文件存在，则继续下载
      // ！！！！！！！有错误，文件已经被ftruncate预分配了文件大小！！！！！！！！！！
      // ！！！！！！！因此is_exist.st_size == task.file_size！！！！！！！！！！！！
      task.handled_size = is_exist.st_size;     // 将断点续传位置定义为文件大小
      respond.header.body_len = PDURESPOND_BODY_BASE_LEN + sizeof(uint64_t);
      respond.msg_len = sizeof(uint64_t);       // 保存已经上传位置到回复体，告诉客户端从哪里开始传输
      uint64_t handled_size = htonll(task.handled_size.load());
      respond.msg.append((char*)&handled_size, sizeof(handled_size));
    }
    else {  // 否则，从零开始
      respond.status = Status::PUT_CONTINUE_FAILED; // 从零开始
      task.handled_size = 0;
    }
  }

  // 打开文件
  int file_fd = open(open_file_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0666);  //打开文件
  if (file_fd == -1) {
    perror("open file in puts error");
    respond.status = Status::FAILED;
    return task;
  }

  // 预分配文件大小
  int ret = ftruncate(file_fd, task.file_size);
  if (ret == -1) {
    perror("ftruncate error");
    close(file_fd);
    respond.status = Status::FAILED;
    return task;
  }

  // 将文件映射到进程的虚拟内存中，修改内存会同步到文件内容
  char *pmap = (char *)mmap(NULL, task.file_size, PROT_WRITE, MAP_SHARED, file_fd, 0);   //偏移量从0开始

  if (pmap == MAP_FAILED) {
    perror("mmap error");
    close(file_fd);
    respond.status = Status::FAILED;
    return task;
  }

  task.file_fd = file_fd;
  task.file_map = pmap;

  return task;
}


//*******************************************上传数据*******************************************//
PutsDataTool::PutsDataTool(AbstractCon *conn) : conn_(dynamic_cast<UpDownCon*>(conn)) {
  
}

PutsDataTool::PutsDataTool(const TranDataPdu& pdu, AbstractCon *conn) : pdu_(pdu), conn_(dynamic_cast<UpDownCon*>(conn)) {
  
}

// 上传文件数据
int PutsDataTool::doingTask() {
  if (conn_->getStatus() == UpDownCon::DOING && conn_->getIsVerify()) {
    recvFileData(conn_);  // 接收文件数据
  }

  if(conn_->getStatus() == UpDownCon::CLOSE) {  //关闭状态
    // conn_->close();
    return -1;
  }

  if (!conn_->getIsVerify()) {
    // 告诉客户端重新验证
    
  }

  return 0;
}

// 接受客户端的数据
void PutsDataTool::recvFileData(UpDownCon *conn) {
  size_t total = conn->getTaskFileSize(); // 文件总大小
  size_t offset = pdu_.file_offset;       // 偏移量
  size_t target_bytes = pdu_.chunk_size;  // 本次希望处理的字节数
  if (pdu_.data.size() < target_bytes) {
    std::cout << "upload recv data: error: the actual data is not in line with expectations" << std::endl;
    return;
  }
  // 如果本次传输数据 + 已接收数据 > 总数据（理论上不会出现，出现说明客户端发送数据错误，大概率是设计问题）
  // 否则，task_.handled_size += target_bytes，该操作为原子操作
  if (!conn->lessEqualAddTaskHandleSize(total - target_bytes, target_bytes)) {
    std::cout << "upload recv data: error: the number data does not match" << std::endl;
    return;
  }
  // 写入数据
  memcpy(conn->getTaskFileMap() + offset, pdu_.data.data(), target_bytes);

  // 发送回复，告诉客户端，接收了那个chunk
  PDURespond res;
  res.header.type = ProtocolType::PDURESPOND_TYPE;
  res.code = Code::PUTS_DATA;
  res.status = Status::SUCCESS;
  res.msg_amount = 1;
  // 设置chunk id
  uint32_t chunk_id = htonl(pdu_.chunk_index);
  res.msg_len = sizeof(chunk_id);
  res.header.body_len = PDURESPOND_BODY_BASE_LEN + res.msg_len;
  res.msg.assign((char*)&chunk_id, sizeof(chunk_id));

  // 发送回复通知客户端已经接收了哪个chunk
  {
    // 由于可能会有多个线程同时发送数据，因此加锁保护
    std::lock_guard<std::mutex> lock(conn->getSendMutex());
    sr_tool_.sendPDURespond(conn->getSSL(), res);
  }
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!! 可以在UDTask中添加，已经接收的chunk id，这样就不需要handled_size !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  // 更新状态
  // 可能会有多个线程到达此时，判断为真，但没有影响
  // !!!!!!!!!!!!!!!!!!!!!!!!!! 这种状态转换是错误的，如果客户端重传数据，也会造成handled_size == total，后续使用chunk id 判断是否完成 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if (conn->getTaskHandledSize() == total) {
    // 因为可能有多个线程同时到达此时，但我们只允许一个线程执行
    // 如果status_是DOING，则将它变为FIN，继续执行
    // 如果status_不是DOING，则直接结束
    // 整个过程是原子的，因此从DOING到FIN只会修改一次，也就是只有一个线程能执行
    if (!conn->cmpExchange(UpDownCon::UDStatus::DOING, UpDownCon::UDStatus::FIN)) {
      return;
    }
    std::cout << "upload file: recv file data finish" << std::endl;

    // 插入数据库，并发送回复
    MyDB db;
    std::string suffix = getSuffix(conn_->getTaskFileName());
    // 插入数据到数据库，并修改已使用空间
    uint64_t ret = db.insertFileData(conn_->getUser(), conn_->getTaskFileName(), conn_->getTaskFileMd5(), conn_->getTaskFileSize(), conn_->getTaskParentDirId(), suffix);

    PDURespond res;
    res.header.type = ProtocolType::PDURESPOND_TYPE;
    res.header.body_len = PDURESPOND_BODY_BASE_LEN;
    res.code = Code::PUTS_FINISH;
    if(ret != 0) {
      res.status = Status::SUCCESS;
      res.msg_amount = 1;
      res.header.body_len = PDURESPOND_BODY_BASE_LEN + sizeof(ret);
      res.msg_len = sizeof(ret);
      ret = htonll(ret);
      res.msg.assign((char*)&ret, sizeof(ret));
    }
    else {
      res.status = Status::FAILED;
    }

    // 发送回复
    {
      // 理论上不会有多个线程同时调用，但任然加锁
      std::lock_guard<std::mutex> lock(conn->getSendMutex());
      sr_tool_.sendPDURespond(conn_->getSSL(), res);
    }
  }
}

//*******************************************上传完成*******************************************//
PutsFinishTool::PutsFinishTool(AbstractCon* conn) : conn_(dynamic_cast<UpDownCon*>(conn)) {

}

PutsFinishTool::PutsFinishTool(const TranFinishPdu &pdu, AbstractCon* conn) : pdu_(pdu), conn_(dynamic_cast<UpDownCon*>(conn)) {

}

int PutsFinishTool::doingTask() {
  if(conn_->getStatus() == UpDownCon::FIN) {   // 完成
    if (conn_->getTaskFileSize() != pdu_.file_size || conn_->getTaskFileMd5() != std::string(pdu_.file_md5)) {
      // 验证错误
      // !!!!!!!!!!!!!!!!!!!!!!!!!!! 处理验证错误的情况 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      LOG_INFO("client %s puts error:%s", conn_->getUser().c_str(), conn_->getTaskFileName().c_str());
    }
    else {
      LOG_INFO("client %s puts:%s", conn_->getUser().c_str(), conn_->getTaskFileName().c_str());
    }
    // !!!!!!!!!!!!!!!!!!!!!!!!!! 完成后为什么只关闭SSL !!!!!!!!!!!!!!!!!!!!!!!!!!!!
    conn_->setStatus(UpDownCon::CLOSE); //设置为关闭状态
  }

  if(conn_->getStatus() == UpDownCon::CLOSE) {  //关闭状态
    // conn_->close();
  }

  return 0;
}

//*******************************************下载任务*******************************************//
GetsTool::GetsTool(AbstractCon *conn) : conn_(dynamic_cast<UpDownCon*>(conn)) {

}

GetsTool::GetsTool(const TranPdu &pdu, AbstractCon *conn) : pdu_(pdu), conn_(dynamic_cast<UpDownCon*>(conn)) {

}

// 下载
int GetsTool::doingTask() {
  if (conn_->getStatus() == UpDownCon::UDStatus::START) {
    firstCheck();  // 首次连接认证
  }

  if (conn_->getStatus() == UpDownCon::CLOSE) {
    // conn_->close();  // 关闭连接
  }
  return 0;
}

// 首次连接认证函数
bool GetsTool::firstCheck() {
  MyDB db;
  PDURespond respond;
  respond.header.type = ProtocolType::PDURESPOND_TYPE;
  respond.header.body_len = PDURESPOND_BODY_BASE_LEN;
  respond.code = Code::GETS;
  respond.msg_amount = 0;
  respond.msg_len = 0;
  UserInfo info;
  bool sql_res = db.getUserInfo(pdu_.user, pdu_.pwd, info);

  if (sql_res && std::string(info.cipher) != "") { // 用户名密码认证通过
    UDtask task;
    if (createTask(respond, task, db)) {
      conn_->init(info, task);    // 设置下载文件信息
      conn_->setVerify(true);     // 设置验证通过
      // 将文件大小和md5发送回给客户端，方便客户端下载完成后进行检查，是否正常
      uint64_t file_size = htonll(task.file_size);
      respond.msg.clear();
      respond.msg.append((char*)&file_size, sizeof(file_size));
      respond.msg.append(conn_->getTaskFileMd5());
      respond.msg_len = sizeof(uint64_t) + respond.msg.size();
      respond.header.body_len = PDURESPOND_BODY_BASE_LEN + respond.msg_len;

      conn_->setStatus(UpDownCon::UDStatus::DOING); // 设置为进行中状态
      conn_->initStatusControl(); // 初始化状态控制（用于控制下载状态）
    }
  }
  else {  // 不存在该用户或密码错误或为通过认证
    respond.status = Status::FAILED;
  }

  // 发送回复
  {
    // 理论上不会有多个线程同时调用，但任然加锁
    std::lock_guard<std::mutex> lock(conn_->getSendMutex());
    sr_tool_.sendPDURespond(conn_->getSSL(), respond);  // 发送认证结果
  }
  if (respond.status != Status::SUCCESS) {
    conn_->setStatus(UpDownCon::UDStatus::CLOSE);
  }

  return true;
}

// 创建下载任务函数
bool GetsTool::createTask(PDURespond &respond, UDtask &task, MyDB &db) {

  task.task_type = UpDownCon::ConType::GETTASK;    // 设置为下载任务
  task.file_name = pdu_.file_name;                 // 任务文件名

  // 查询文件是否存在，并返回MD5码
  // 查询文件MD5码，不存在返回false，parent_dir_id指的是要下载的文件ID
  // pdu_的parent_dir_id字段实际保存的是file_id
  if(!db.getFileMd5(pdu_.user, pdu_.parent_dir_id, task.file_md5)) {
    respond.status = Status::FILE_NOT_EXIST;
    return false;
  }

  std::string full_path = std::string(ROOTFILEPATH) + "/" + std::string(pdu_.user) + "/" + task.file_md5;  // 用户目录下的文件路径
  struct stat file_stat;

  if (stat(full_path.c_str(), &file_stat) == -1) {  // 文件不存在
    respond.status = Status::FILE_NOT_EXIST;
    return false;
  }

  task.file_size = file_stat.st_size; // 文件总大小

  // 断点续传或者从新开始
  if(pdu_.sended_size < task.file_size) {
    task.handled_size = pdu_.sended_size;       // 客户端指定的偏移量，但是不能大于文件总字节.默认为0
  }
  else {
    respond.status = Status::GET_CONTINUE_FAILED; // 断点下载失败
    return false;
  }

  // 打开文件以便读取
  int file_fd = open(full_path.c_str(), O_RDONLY);
  if (file_fd == -1) {
    perror("open file in gets error");
    respond.status = Status::FAILED;
    return false;
  }

  // 映射文件到内存
  char *pmap = (char *)mmap(NULL, task.file_size, PROT_READ, MAP_SHARED, file_fd, 0);  // 内存映射
  if (pmap == MAP_FAILED) {
    perror("mmap error");
    close(file_fd);
    respond.status = Status::FAILED;
    return false;
  }

  task.file_fd = file_fd;
  task.file_map = pmap;

  respond.status = Status::SUCCESS; // 设置为OK
  return true;
}


//*******************************************下载数据*******************************************//
GetsDataTool::GetsDataTool(AbstractCon *conn) : conn_(dynamic_cast<UpDownCon*>(conn)) {

}

GetsDataTool::GetsDataTool(const TranDataPdu &pdu, AbstractCon *conn) : pdu_(pdu), conn_(dynamic_cast<UpDownCon*>(conn)) {

}

int GetsDataTool::doingTask() {
  if (conn_->getStatus() == UpDownCon::UDStatus::DOING && conn_->getIsVerify()) {
    sendFileData();  // 发送文件数据
  }

  if (!conn_->getIsVerify()) {
    // 发送重新验证回复
    return -1;
  }
  
  if (conn_->getStatus() == UpDownCon::UDStatus::FIN) {
    conn_->client_type = AbstractCon::GETTASKWAITCHECK;   // 更改连接类型为等待确认，只监听可读事件，不再监听可写事件  
    return 0;
  }

  if(conn_->getStatus() == UpDownCon::CLOSE) {  //关闭状态
    // conn_->close();
  }

  return -1;
}
 
// 发送数据
void GetsDataTool::sendFileData() {
  SSL *ssl = conn_->getSSL();
  if (ssl == nullptr) {
    conn_->setStatus(UpDownCon::CLOSE);
    return;
  }

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 这里直接发送了所有数据，但并不能确认客户端接收完了 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 不能确定客户端接收的数据，也就不能重传，后续可添加 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // 发送文件数据
  // !!!!!!!!!!!!!!!!!!!!!!!! 这里获取之前下载的文件数据量在当前无用，如果后续添加离线任务续传的功能，可以使用它 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  size_t pre_handled_bytes = conn_->getTaskHandledSize();   // 之前处理的字节
  size_t total = conn_->getTaskFileSize() - pre_handled_bytes;  // 需要传输的总字节数
  size_t chunk_size = 2048;   // 每次发送的块大小

  size_t total_chunks = (total-1)/chunk_size + 1; // 总chaunk数，向上取整
  size_t last_chunk_size = total - chunk_size*(total_chunks-1); // 最后一个chunk的大小
  // 创建发送数据协议
  TranDataPdu tran_data;
  tran_data.header.type = ProtocolType::TRANDATAPDU_TYPE;
  tran_data.code = Code::GETS_DATA;
  // 设置并发送文件数据
  for (uint32_t i=0; i<total_chunks; ++i) {
    tran_data.file_offset = (uint64_t)i * chunk_size + pre_handled_bytes;
    tran_data.chunk_size = (i == total_chunks-1 ? last_chunk_size : chunk_size);
    tran_data.total_chunks = total_chunks;
    tran_data.chunk_index = i;
    tran_data.data.assign(conn_->getTaskFileMap() + tran_data.file_offset, tran_data.chunk_size);
    // body长度为，TranDataPdu基础长度+数据长度
    tran_data.header.body_len = TRANDATAPDU_BODY_BASE_LEN + tran_data.chunk_size;

    // 传输控制
    if (conn_->getStatus() == UpDownCon::UDStatus::PAUSE) { // 暂停
      // 等待状态再次改变，且不为PAUSE
      conn_->wait([this]() { return conn_->getStatus() != UpDownCon::UDStatus::PAUSE; });
    }
    if (conn_->getStatus() == UpDownCon::UDStatus::CLOSE) { // 取消
      break;
    }
    // 继续，则什么都不做，继续
    
    // 避免长时间占用cpu
    if (conn_->getIsVip()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    else {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 发送数据
    size_t send_bytes = 0;
    {
      // 理论上不会有多个线程同时调用，但任然加锁
      std::lock_guard<std::mutex> lock(conn_->getSendMutex());
      send_bytes = sr_tool_.sendTranDataPdu(conn_->getSSL(), tran_data);
    }
    if (send_bytes != PROTOCOLHEADER_LEN + tran_data.header.body_len) {
      std::cout << "download file: send data error" << std::endl;
      break;
    }
    conn_->addTaskHandleSize(tran_data.chunk_size); // 更新处理字节数
  }

  // !!!!!!!!!!!!!!!!!!!!!!!!!! 这里根据服务端发送的数据判断是否完成，实际应该根据客户端接收到的数据判断 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // 检查文件是否已经全部发送
  if (conn_->getTaskHandledSize() == total) {
    conn_->setStatus(UpDownCon::FIN);  // 文件传输完成
  }
  else{ // 没有发送所有数据，错误
    conn_->setStatus(UpDownCon::CLOSE);
  }
}


//*******************************************下载完成*******************************************//
GetsFinishTool::GetsFinishTool(AbstractCon *conn) : conn_(dynamic_cast<UpDownCon*>(conn)) {

}

GetsFinishTool::GetsFinishTool(const TranFinishPdu &pdu, AbstractCon *conn) : pdu_(pdu), conn_(dynamic_cast<UpDownCon*>(conn)) {

}

int GetsFinishTool::doingTask() {
  if (conn_->getStatus() == UpDownCon::FIN) {
    if (pdu_.file_size == 1) {  // 客户端验证成功
      // 发送回复
      PDURespond res;
      res.header.type = ProtocolType::PDURESPOND_TYPE;
      res.header.body_len = PDURESPOND_BODY_BASE_LEN;
      res.code = GETS_FINISH;
      res.status = SUCCESS;
      res.msg_len = 0;
      // 发送
      {
        // 理论上不会有多个线程同时调用，但任然加锁
        std::lock_guard<std::mutex> lock(conn_->getSendMutex());
        sr_tool_.sendPDURespond(conn_->getSSL(), res);
      }

      // 关闭连接
      conn_->setStatus(UpDownCon::CLOSE);
      LOG_INFO("client %s gets: %s", conn_->getUser().c_str(), conn_->getTaskFileName().c_str());
    }
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 失败情况未处理 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    else {  // 客户端验证失败
      // 可以在这里实现检验出错，处理客户重传，重新把状态机改成DOING。连接类型重新设置为GETS暂不实现
      // 发送回复
      PDURespond res;
      res.header.type = ProtocolType::PDURESPOND_TYPE;
      res.header.body_len = PDURESPOND_BODY_BASE_LEN;
      res.code = GETS_FINISH;
      res.status = FAILED;
      res.msg_len = 0;
      // 发送
      {
        // 理论上不会有多个线程同时调用，但任然加锁
        std::lock_guard<std::mutex> lock(conn_->getSendMutex());
        sr_tool_.sendPDURespond(conn_->getSSL(), res);
      }


      conn_->setStatus(UpDownCon::CLOSE);
      LOG_INFO("client %s gets failed: %s", conn_->getUser().c_str(), conn_->getTaskFileName().c_str());
    }
  }
  
  if (conn_->getStatus() == UpDownCon::CLOSE) {
    // conn_->close();  // 关闭连接
  }
  return 0;
}


//*******************************************下载控制*******************************************//
GetsControlTool::GetsControlTool(AbstractCon *conn) : conn_(dynamic_cast<UpDownCon*>(conn)) {

}

GetsControlTool::GetsControlTool(const TranControlPdu &pdu, AbstractCon *conn) : pdu_(pdu), conn_(dynamic_cast<UpDownCon*>(conn)) {

}

int GetsControlTool::doingTask() {
  // 只能再状态为进行和暂停时更改
  if ((conn_->getStatus() == UpDownCon::UDStatus::PAUSE || conn_->getStatus() == UpDownCon::UDStatus::DOING) && conn_->getIsVerify()) {
    // 根据action改变conn_.status_
    switch (pdu_.action) {
      case ControlAction::PAUSE: {
        conn_->setStatus(UpDownCon::UDStatus::PAUSE);
        break;
      }
      case ControlAction::RESUME: {
        conn_->setStatus(UpDownCon::UDStatus::DOING);
        conn_->notifyOne();
        break;
      }
      case ControlAction::CANCEL: {
        conn_->setStatus(UpDownCon::UDStatus::CLOSE);
        conn_->notifyAll();
        break;
      }
      default: {
        break;
      }
    }
  }

  if (!conn_->getIsVerify()) {
    // 发送重新验证回复
    return -1;
  }

  if(conn_->getStatus() == UpDownCon::CLOSE) {  //关闭状态
    // conn_->close();
  }

  return -1;
}
