#include "ShortTaskTool.h"
#include "Log.h"
#include <vector>

// 登录
LoginTool::LoginTool(const PDU &pdu, AbstractCon *conn) : pdu_(pdu), conn_parent_(conn) {

}

// 用户登录
int LoginTool::doingTask() {
  std::cout << "LoginTool: doingTask()" << std::endl;
  //查找用户信息，确定用户存在
  PDURespond respond;
  respond.header.type = ProtocolType::PDURESPOND_TYPE;
  respond.header.body_len = PDURESPOND_BODY_BASE_LEN;
  respond.code = Code::SIGNIN;
  UserInfo info;
  MyDB db;
  ClientCon *conn = dynamic_cast<ClientCon*>(conn_parent_);
  bool sql_res = db.getUserInfo(pdu_.user, pdu_.pwd, info);

  if(sql_res && std::string(info.cipher) != "") {
    respond.status = Status::SUCCESS; //返回正确
    // 设置用户信息
    respond.msg_amount = 1;
    respond.msg_len = USERSCOLLEN*USERSCOLMAXSIZE;
    respond.header.body_len = PDURESPOND_BODY_BASE_LEN + respond.msg_len;
    respond.msg.append(info.user, USERSCOLMAXSIZE);
    respond.msg.append(info.pwd, USERSCOLMAXSIZE);
    respond.msg.append(info.cipher, USERSCOLMAXSIZE);
    respond.msg.append(info.is_vip, USERSCOLMAXSIZE);
    respond.msg.append(info.capacity_sum, USERSCOLMAXSIZE);
    respond.msg.append(info.used_capacity, USERSCOLMAXSIZE);
    respond.msg.append(info.salt, USERSCOLMAXSIZE);
    respond.msg.append(info.vip_date, USERSCOLMAXSIZE);

    conn->init(info);      //保存客户信息
    conn->setVerify(true); //设置客户端已经通过认证
  }
  else {
    respond.status = Status::FAILED;  //返回错误
  }

  // 发送回复
  {
    std::lock_guard<std::mutex> lock(conn->getSendMutex());
    sr_tool_.sendPDURespond(conn->getSSL(), respond); //发送回客户端
  }

  if(respond.status == Status::SUCCESS) {  // 登录成功
    LOG_INFO("User:%s Login", pdu_.user);
    return 0;
  }
  else {
    return -1;
  }
}


// 注册
SignTool::SignTool(const PDU &pdu, AbstractCon *conn) : pdu_(pdu), conn_parent_(conn) {

}

// 注册用户
int SignTool::doingTask() {
  std::cout << "SignTool: doingTask()" << std::endl;
  PDURespond respond;   // 回复体
  respond.header.type = ProtocolType::PDURESPOND_TYPE;
  respond.header.body_len = PDURESPOND_BODY_BASE_LEN;
  respond.code = Code::SIGNUP;
  UserInfo info;        // 客户信息
  MyDB db;              // 数据库连接
  ClientCon * conn = dynamic_cast<ClientCon*>(conn_parent_);

  // 查询是否存在该用户
  bool sql_res = db.getUserExist(pdu_.user);
  if(!sql_res) {  // 用户不存在，则创建
    std::string ciper = generateHash(pdu_.user, pdu_.pwd);  //生成一个哈希密文
    sql_res = db.insertUser(pdu_.user, pdu_.pwd, ciper);    //插入用户，其它字段为默认值
    if(sql_res) {
      sql_res = createDir();     //创建用户根文件夹
    }
    if(sql_res) {
      respond.status = Status::SUCCESS;
      db.getUserInfo(pdu_.user, pdu_.pwd, info);      //获取用户信息
      // 设置用户信息
      respond.msg_amount = 1;
      respond.msg_len = USERSCOLLEN*USERSCOLMAXSIZE;
      respond.header.body_len = PDURESPOND_BODY_BASE_LEN + respond.msg_len;
      respond.msg.append(info.user, USERSCOLMAXSIZE);
      respond.msg.append(info.pwd, USERSCOLMAXSIZE);
      respond.msg.append(info.cipher, USERSCOLMAXSIZE);
      respond.msg.append(info.is_vip, USERSCOLMAXSIZE);
      respond.msg.append(info.capacity_sum, USERSCOLMAXSIZE);
      respond.msg.append(info.used_capacity, USERSCOLMAXSIZE);
      respond.msg.append(info.salt, USERSCOLMAXSIZE);
      respond.msg.append(info.vip_date, USERSCOLMAXSIZE);

      conn->init(info);          //用客户信息保存在连接类中
      conn->setVerify(true);     //标志为已经通过认证客户端
    }
    else {
      respond.status = Status::FAILED;
    }
  }
  else {  // 用户存在，不能创建，错误
    respond.status = Status::FAILED;
  }

  // 发送回复
  {
    std::lock_guard<std::mutex> lock(conn->getSendMutex());
    sr_tool_.sendPDURespond(conn->getSSL(), respond);
  }
  if(respond.status == Status::SUCCESS) {
    LOG_INFO("User:%s Sgin", pdu_.user);
    return 0;
  }
  return -1;
}

//创建用户根文件夹
bool SignTool::createDir() {
  char temp[1024] = {0};
  std::string path;
  mode_t mode = 0755;  // 文件夹的权限

  // 获取当前工作目录
  if (getcwd(temp, sizeof(temp)) != NULL) {
    path = std::string(temp);
    std::cout << "cur path:" << path << std::endl;
  }
  else {
    return false;
  }

  // 将用户名称拼接到路径后
  path += "/" + std::string(ROOTFILEPATH) + ("/" + std::string(pdu_.user));

  struct stat info;

  // 检查文件/文件夹是否存在
  if (stat(path.c_str(), &info) != 0) {
    // 文件不存在，尝试创建文件夹
    int result = mkdir(path.c_str(), mode);
    if (result != 0) {   // mkdir 返回非 0 错误，返回 0 成功
      LOG_INFO("mkdir error %s", pdu_.user);
    }
    return result == 0;
  } 
  else if (info.st_mode & S_IFDIR) {
    return true;  // 如果已经存在，可以共有，返回true
  } 

  LOG_INFO("mkdir error %s", pdu_.user);
  return false;
}

// 返回给客户端文件列表功能
CdTool::CdTool(const PDU &pdu,AbstractCon *conn) : pdu_(pdu), conn_parent_(conn) {

}

// 执行向客户端传输文件信息任务
int CdTool::doingTask() {
  std::cout << "CdTool: doningTask()" << std::endl;
  // 创建回复体
  PDURespond respond;
  respond.header.type = ProtocolType::PDURESPOND_TYPE;
  respond.header.body_len = PDURESPOND_BODY_BASE_LEN;
  respond.code = Code::CD;
  respond.msg_amount = 0;
  respond.msg_len = 0;
  ClientCon *conn = dynamic_cast<ClientCon*>(conn_parent_);
  SSL *ssl = conn->getSSL();
  if(!conn->getIsVerify()) {  // 如果客户端没认证
    respond.status = Status::NOT_VERIFY;  // 返回告诉客户端先进行登陆操作
    {
      std::lock_guard<std::mutex> lock(conn->getSendMutex());
      sr_tool_.sendPDURespond(ssl, respond);
    }
    return -1;
  }
  // 执行数据库操作
  MyDB db;
  std::vector<FileInfo> file_vet;
  bool sql_res = db.getUserAllFileInfo(conn->getUser(), file_vet);
  if(!sql_res) {  // 失败
    respond.status = Status::FAILED;  // 发送错误回去给客户端
    {
      std::lock_guard<std::mutex> lock(conn->getSendMutex());
      sr_tool_.sendPDURespond(ssl, respond);
    }
    return -1;
  }

  // 正常发送全部文件信息
  // !!!!!!!!!!!!!!!!!!!!!! 这里直接发送回复后，另外发送文件信息 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // 将回复体发送回客户端
  uint32_t file_cnt = file_vet.size();
  file_cnt = htonl(file_cnt);
  respond.status = Status::SUCCESS;
  respond.msg_amount = 1;
  respond.msg_len = sizeof(file_cnt);
  respond.header.body_len = PDURESPOND_BODY_BASE_LEN + respond.msg_len;
  respond.msg.append((char*)&file_cnt, sizeof(file_cnt));

  // 发送回复
  {
    std::lock_guard<std::mutex> lock(conn->getSendMutex());
    sr_tool_.sendPDURespond(ssl, respond);
  }

  // 发送文件信息
  {
    std::lock_guard<std::mutex> lock(conn->getSendMutex());
    sr_tool_.sendFileInfo(ssl, file_vet);
  }
  LOG_INFO("client %s cd",conn->getUser().c_str());
  return 0;
}


// 创建文件夹
CreateDirTool::CreateDirTool(const PDU &pdu, AbstractCon *conn) : pdu_(pdu), conn_(static_cast<ClientCon*>(conn)) {

}

int CreateDirTool::doingTask() {
  std::cout << "CreateDirTool: doingTask()" << std::endl;
  PDURespond respond;
  respond.header.type = ProtocolType::PDURESPOND_TYPE;
  respond.header.body_len = PDURESPOND_BODY_BASE_LEN;
  respond.code = Code::MAKEDIR;
  SSL *ssl = conn_->getSSL();

  // 如果客户端未认证
  if (!conn_->getIsVerify()) {
    respond.status = Status::NOT_VERIFY;    // 未验证
    {
      std::lock_guard<std::mutex> lock(conn_->getSendMutex());
      sr_tool_.sendPDURespond(ssl, respond);
    }
    return -1;
  }

  // 设置数据库及任务信息
  MyDB db;
  // 获取parent_dir_id
  uint64_t parent_dir_id = 0;
  memcpy((char*)&parent_dir_id, pdu_.msg, sizeof(parent_dir_id));
  parent_dir_id = ntohll(parent_dir_id);
  // 插入文件夹数据
  uint64_t new_id = db.insertFileData(conn_->getUser(), pdu_.file_name, "", 0, parent_dir_id, "d");
  if (new_id != 0) {
    // 依次保存new_id，parent_id，new_dir_name
    respond.status = Status::SUCCESS;
    respond.msg_amount = 1;
    respond.msg_len = sizeof(new_id) + sizeof(parent_dir_id) + sizeof(pdu_.file_name);
    respond.header.body_len = PDURESPOND_BODY_BASE_LEN + respond.msg_len;
    new_id = htonll(new_id);  // 转换字节序
    parent_dir_id = htonll(parent_dir_id);
    respond.msg.clear();
    respond.msg.append((char*)&new_id, sizeof(new_id));
    respond.msg.append((char*)&parent_dir_id, sizeof(parent_dir_id));
    respond.msg.append(pdu_.file_name, sizeof(pdu_.file_name));
  }
  else {
    respond.status = Status::FAILED;  // 返回数据库插入失败的错误码
  }

  // 发送响应
  {
    std::lock_guard<std::mutex> lock(conn_->getSendMutex());
    sr_tool_.sendPDURespond(ssl, respond);
  }

  LOG_INFO("client %s created directory: %s", conn_->getUser().c_str(), pdu_.file_name);
  return 0;
}


// 删除文件或文件夹
DeleteTool::DeleteTool(const PDU &pdu, AbstractCon *conn) : pdu_(pdu), conn_(static_cast<ClientCon*>(conn)) {

}

int DeleteTool::doingTask() {
  std::cout << "DeleteTool: doingTask()" << std::endl;
  PDURespond respond;
  respond.header.type = ProtocolType::PDURESPOND_TYPE;
  respond.header.body_len = PDURESPOND_BODY_BASE_LEN;
  respond.code = Code::DELETEFILE;
  SSL *ssl = conn_->getSSL();

  // 检查 SSL 连接是否有效
  if (ssl == nullptr) {
    std::cerr << "SSL connection is not initialized." << std::endl;
    return -1;
  }

  // 如果客户端未认证
  if (!conn_->getIsVerify()) {
    respond.status = Status::NOT_VERIFY;  // 未验证
    {
      std::lock_guard<std::mutex> lock(conn_->getSendMutex());
      sr_tool_.sendPDURespond(ssl, respond);
    }
    return -1;
  }

  // 数据库处理
  MyDB db;
  std::string suffix = getSuffix(pdu_.file_name); // 获取后缀名
  bool ret = false;
  uint64_t file_id = 0;

  // 这里把pdu的pwd字段赋给file_id，是因为客户端发送的file_id保存在pwd字段中
  memcpy((char*)&file_id, pdu_.pwd, sizeof(uint64_t));
  file_id = ntohll(file_id);  // 转换字节序

  // 根据文件类型删除文件或文件夹
  if (suffix == "d") { // 如果类型是文件夹
    ret = db.deleteOneDir(conn_->getUser(), file_id);
  }
  else {
    ret = db.deleteOneFile(conn_->getUser(), file_id);
  }

  // 设置响应代码
  if (ret) {
    respond.status = Status::SUCCESS;
  }
  else {
    respond.status = Status::FAILED;
    std::cerr << "Failed to delete " << (suffix == "d" ? "directory" : "file") << " with ID: " << file_id << std::endl;
  }

  // 发送响应
  {
    std::lock_guard<std::mutex> lock(conn_->getSendMutex());
    sr_tool_.sendPDURespond(ssl, respond);
  }
  return 0;
}
