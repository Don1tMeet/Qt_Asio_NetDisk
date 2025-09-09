#include "MyDB.h"
#include "SqlConnPool.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <iomanip>

// 初始化静态成员
std::string MyDB::table_name = "Users";
std::string MyDB::columns_1 = "User";
std::string MyDB::columns_2 = "Password";


MyDB::MyDB() {
  conn_RAII_ = new SqlConnRAII(conn_, SqlConnPool::getInstance());
}

MyDB::~MyDB() {
  if (conn_RAII_) {
    delete conn_RAII_;
  }
}

// 传递用户名和密码，用户信息保存在info
bool MyDB::getUserInfo(const std::string &user, const std::string &pwd, UserInfo &info) {
  std::string sql = "SELECT * FROM " + table_name + " WHERE " + columns_1 + "=? AND " + columns_2 + "=? LIMIT 1";
  std::vector<std::string> params = {user, pwd};
  std::vector<std::string> result;
  int sql_res = executeSelect(sql, params, result);  // 根据查询结果更新info
  if (sql_res > 0) {
    return ConvertTouserInfo(result, info);
  }
  return false;
}

// 查询用户是否存在
bool MyDB::getUserExist(const std::string &user) {
  std::string sql = "SELECT User FROM " + table_name + " WHERE " + columns_1 + " = ?";
  std::vector<std::string> params = {user};
  std::vector<std::string> result;
  int sql_res = executeSelect(sql, params, result);

  return sql_res > 0;
}

// 传入用户名，密码，哈希密文创建用户
bool MyDB::insertUser(const std::string &user, const std::string &pwd, const std::string &cipher) {
  std::string sql = "INSERT INTO Users (User, Password, Cipher) VALUES (?, ?, ?)";
  std::vector<std::string> params = {user, pwd, cipher};
  return executeAlter(sql, params) > 0;
}

// 传入所属用户名、文件后缀名、MD5、该文件所属父目录ID，插入该文件。不存在父目录，插入根目录，默认为0，返回插入数据库中文件的ID
std::uint64_t MyDB::insertFileData(const std::string &user, const std::string file_name, const std::string file_md5, const uint64_t file_size, const uint64_t parent_dir_id, const std::string &suffix) {
  //1、检查父ID是否存在
  std::string sql = "select DirGrade FROM FileDir where User=? and Fileid = ? and FileType ='d'";    
  std::vector<std::string> params;
  std::vector<std::string> ret;
  params.push_back(user);
  params.push_back(std::to_string(parent_dir_id));

  int dir_grade = 0; //文件夹距离根文件夹距离。
  if(executeSelect(sql, params, ret) > 0) { //存在父文件夹
    dir_grade = stoi(ret[0]) + 1; //等级为父文件夹等级+1
  }
  else {  //不存在该文件夹，将其保存在根文件夹，如果存在，返回文件夹等级
    dir_grade = 0;
    //task->parent_dir_id.store(0);
  }

  //2、查询文件当前最大ID
  sql = "SELECT Fileid FROM FileDir WHERE User=? ORDER BY Fileid DESC LIMIT 1";
  params.resize(1);
  params[0] = user;
  ret.clear();
  uint64_t fileid = 0;
  if(executeSelect(sql, params, ret) > 0) {
    fileid = std::stoull(ret[0]) + 1;   //当前最大ID+1
  }
  else {
    fileid = 1;  //初始化为1
  }

  //3、插入文件信息到文件表
  sql = "INSERT INTO FileDir (Fileid,User,FileName,DirGrade,FileType,MD5,FileSize,ParentDir) VALUES(?,?,?,?,?,?,?,?)";
  params.clear();
  params.push_back(std::to_string(fileid));
  params.push_back(user);
  params.push_back(file_name);
  params.push_back(std::to_string(dir_grade));
  params.push_back(suffix);      //d是文件夹，其它文件夹
  params.push_back(file_md5);
  params.push_back(std::to_string(file_size));
  params.push_back(std::to_string(parent_dir_id));

  if(executeAlter(sql, params) <= 0) {
    perror("Insert FileDir database failed");
    return 0;
  }


  params.clear();
  //4、更新已用空间
  sql = "UPDATE Users SET usedCapacity=usedCapacity+? where User=?";
  params.push_back(std::to_string(file_size));
  params.push_back(std::string(user));

  if(executeAlter(sql, params) < 0) {
    perror("Updata FileDir database failed");
    return 0;
  }

  return fileid;
}

// 删除单个文件
bool MyDB::deleteOneFile(const std::string &user, std::uint64_t &file_id) {
  if (file_id == 0) {
    return false;
  }

  // 先保存该文件的MD5码
  std::string md5;
  if (!getFileMd5(user, file_id, md5)) {  // 文件不存在，返回false
    return false;
  }

  // 查找相同MD5在数据库中的数量
  std::string sql = "SELECT FileSize FROM FileDir WHERE User=? AND MD5=?";
  std::vector<std::string> parms = { user, md5 };
  std::vector<std::vector<std::string>> file_count;

  if (executeSelect(sql, parms, file_count) <= 0) {
    return false;
  }

  //从数据库中删除
  sql = "DELETE FROM FileDir WHERE Fileid=? AND User=? AND MD5=?";
  parms = { std::to_string(file_id), user, md5 };

  if (executeAlter(sql, parms) <= 0) {
    return false;
  }

  // 更新已用空间
  sql = "UPDATE Users SET usedCapacity=usedCapacity-? where User=?";
  parms = { file_count[0][0], user };

  if(executeAlter(sql, parms) < 0) {
    perror("Updata FileDir database failed");
    return 0;
  }

  //如果是最后一个，则删除文件
  if (file_count.size() == 1) {
    std::string file_path = std::string(ROOTFILEPATH) + "/" + user + "/" + md5;
    int fd = open(file_path.c_str(), O_RDONLY);  //尝试以独占打开文件，如果失败说明正在使用，不能删除
    if (fd != -1) {
      close(fd);
      if (remove(file_path.c_str()) == 0) {
        return true;
      }
    }
    std::cerr << "Error deleting file: " << file_path << std::endl;
    return false;  
  }
  return true;
}

//递归删除一个文件夹。性能影响较大，可以采用数据库存储过程等优化
bool MyDB::deleteOneDir(const std::string &user, std::uint64_t &file_id) {
    // 检查 fileid 是否有效
    if (file_id == 0) {
      return false;
    }
    
    // 查询该文件的所有子项
    std::string sql = "SELECT Fileid, FileType FROM FileDir WHERE User=? AND ParentDir=?";
    std::vector<std::string> params{user, std::to_string(file_id)};
    std::vector<std::vector<std::string>> ret;

    if (executeSelect(sql, params, ret) < 0) {  // 这里返回值为0也可以（因为可以删除空文件）
      return false;
    }


    // 递归删除子项
    for (const auto &row : ret) {
      uint64_t cur_file_id = std::stoull(row[0]);
      if (row[1] == "d") {  // 如果是文件夹，递归删除
        if (!deleteOneDir(user, cur_file_id)) {
          std::cerr << "Failed to delete subdirectory: " << cur_file_id << std::endl;
          return false;
        }
      }
      else {
        if (!deleteOneFile(user, cur_file_id)) {
          std::cerr << "Failed to delete file: " << cur_file_id << std::endl;
          return false;
        }
      }
    }

    // 删除本目录
    sql = "DELETE FROM FileDir WHERE Fileid=? AND User=?";
    params = {std::to_string(file_id), user};

    if (executeAlter(sql, params) <= 0) {
      std::cerr << "Failed to delete directory: " << file_id << std::endl;
      return false;
    }

    return true;  // 成功删除
}

//传入用户名，文件id，获取MD5码保存在md5中，成功返回true.
bool MyDB::getFileMd5(const std::string &user, const std::uint64_t &file_id, std::string &md5) {
  std::string sql = "SELECT MD5 FROM FileDir WHERE Fileid=? AND User=? AND FileType!='d'"; //文件夹无法下载，没有MD5码。所以排除文件夹
  std::vector<std::string> parmas(2);
  parmas[0] = std::to_string(file_id);
  parmas[1] = user;

  std::vector<std::string> ret;
  if(executeSelect(sql, parmas, ret) <= 0) {
    return false;
  }
  md5 = std::move(ret[0]);
  return true;
}

// 检查空间是否足够
bool MyDB::getIsEnoughSpace(const std::string &user, const std::string &pwd, std::uint64_t file_size) {
  // 查询用户的总容量和已使用容量
  std::string sql = "SELECT CapacitySum, usedCapacity FROM Users WHERE User=? AND Password=?";

  std::vector<std::string> parmas(2);
  parmas[0] = user;
  parmas[1] = pwd;

  // 存储查询结果
  std::vector<std::string> ret;
  if (executeSelect(sql, parmas, ret) <= 0) { // 用户不存在或密码错误
    return false;
  }

  try {
    // 转换容量值为整数类型
    std::uint64_t total_capacity = std::stoull(ret[0]);
    std::uint64_t used_capacity = std::stoull(ret[1]);

    // 计算剩余容量是否足够
    return (total_capacity - used_capacity) >= file_size;
  }
  catch (const std::invalid_argument &e) {
    // 捕捉无效的转换异常，比如非数字字符
    std::cerr << "Invalid argument: " << e.what() << std::endl;
    return false;
  }
  catch (const std::out_of_range &e) {
    // 捕捉转换时溢出的异常
    std::cerr << "Out of range: " << e.what() << std::endl;
    return false;
  }
}

// 查询是否存在该文件，是否支持秒传。如果存在返回真，否则返回false
bool MyDB::getFileExist(const std::string &user, const std::string &md5) {
  std::string sql = "SELECT Fileid FROM FileDir WHERE User=? AND MD5=?";
  std::vector<std::string> params(2);
  params[0] = user;
  params[1] = md5;

  std::vector<std::string> result;
  return executeSelect(sql, params, result) > 0;
}

//传递用户名，和保存返回结果的文件信息结构体容器，返回用户在数据库中的文件信息。
bool MyDB::getUserAllFileInfo(const std::string &user, std::vector<FileInfo> &vet) {
  vet.clear();

  std::string sql = "SELECT Fileid,FileName,DirGrade,FileType,FileSize,ParentDir,FileDate FROM FileDir WHERE User=?";
  std::vector<std::string> params(1, user);
  std::vector<std::vector<std::string>> ret;

  if(executeSelect(sql, params, ret) <= 0) {
    return false;
  }

  vet.reserve(ret.size());

  //将返回结果保存到vet
  for(size_t i=0; i<ret.size(); ++i) {
    FileInfo file_info;
    if(VetToFileInfo(ret[i], file_info)) {
      vet.emplace_back(file_info);
    }
    else {
      return false;
    }
  }
  return true;
}

// 通用查询，传入sql指令和参数，结果保存在result，只保存一条数据
int MyDB::executeSelect(const std::string &sql, const std::vector<std::string> &params, std::vector<std::string>& result) {
  try {
    result.clear();
    
    std::unique_ptr<sql::PreparedStatement> stmt(conn_->prepareStatement(sql));
    // 设置参数
    for (size_t i = 0; i!=params.size(); ++i) {
      stmt->setString(i+1, params[i]);
    }
    // 执行查询，并获取结果
    std::unique_ptr<sql::ResultSet> sql_res(stmt->executeQuery());

    size_t col_count = sql_res->getMetaData()->getColumnCount();    //获取列数

    // 遍历结果
    while (sql_res->next()) {
      for (size_t i = 1; i<=col_count; ++i) {
        result.push_back(sql_res->getString(i));
      }
      break;
    }
    
    return result.size();
  }
  catch (sql::SQLException& e) {
    std::cerr << "SQL error: " << e.what() << std::endl;
    return -1;
  }
}

// 通用查询，传入sql指令和参数，结果保存在result，保存所有数据
int MyDB::executeSelect(const std::string &sql, const std::vector<std::string> &params, std::vector<std::vector<std::string>>& result) {
  try {
    result.clear();

    std::unique_ptr<sql::PreparedStatement> stmt(conn_->prepareStatement(sql));
    // 设置参数
    for (size_t i=0; i!=params.size(); ++i) {
      stmt->setString(i+1, params[i]);
    }
    // 执行查询，并获取结果
    std::unique_ptr<sql::ResultSet> sql_res(stmt->executeQuery());

    size_t col_count = sql_res->getMetaData()->getColumnCount();    //获取列数

    // 遍历结果
    while (sql_res->next()) {
      std::vector<std::string> row;
      for (size_t i=1; i<=col_count; ++i) {
        row.push_back(sql_res->getString(i));
      }
      result.push_back(std::move(row));
    }
    return result.size();
  }
  catch (sql::SQLException& e) {
    std::cerr << "SQL error: " << e.what() <<std::endl;
    return -1;
  }
}

// 通用更新，插入，删除，传入sql指令和参数
int MyDB::executeAlter(const std::string &sql, const std::vector<std::string> &params) {
  try {
    std::unique_ptr<sql::PreparedStatement> stmt(conn_->prepareStatement(sql));
    // 设置参数
    for (size_t i=0; i!=params.size(); ++i) {
      stmt->setString(i+1, params[i]);
    }
    // 执行
    int sql_res = stmt->executeUpdate();
    return sql_res;
  }
  catch (sql::SQLException& e) {
    std::cerr << "SQL error: " << e.what() << std::endl;
    return -1;
  }
}