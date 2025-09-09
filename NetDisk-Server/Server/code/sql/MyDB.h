#pragma once


#include "SqlConnRAII.h"
#include "protocol.h"
#include "UpDownCon.h"
#include <vector>
#include <string>



class MyDB {
 private:
  static std::string table_name;      // 表名
  static std::string columns_1;       // 字段1
  static std::string columns_2;       // 字段2

 public:
  MyDB();
  ~MyDB();

  // 查询接口
  bool getUserInfo(const std::string &user, const std::string &pwd, UserInfo &info);                //查询用户信息
  bool getUserExist(const std::string &user);                                                       //查询用户是否存在
  bool insertUser(const std::string &user, const std::string &pwd, const std::string &cipher);      //插入用户
  bool getFileMd5(const std::string &user, const std::uint64_t &file_id, std::string &md5);         //获取文件MD5
  bool getIsEnoughSpace(const std::string &user, const std::string &pwd, std::uint64_t file_size);  //检查空间是否足够
  bool getFileExist(const std::string &user, const std::string &md5);                               //查询是否已经存在该文件，支不支持秒传
  bool getUserAllFileInfo(const std::string &user, std::vector<FileInfo> &vet);                     //获取用户在数据库中的全部文件信息

  std::uint64_t insertFileData(const std::string &user, const std::string file_name, const std::string file_md5, const uint64_t file_size, const uint64_t parent_dir_id, const std::string &suffix);   //插入文件数据
  bool deleteOneFile(const std::string &user, std::uint64_t &file_id);                              //删除单个文件
  bool deleteOneDir(const std::string &user, std::uint64_t &file_id);                               //删除文件夹

 private:
  int executeSelect(const std::string &sql, const std::vector<std::string> &params, std::vector<std::string>& result);
  int executeSelect(const std::string &sql, const std::vector<std::string> &params, std::vector<std::vector<std::string>>& result);
  int executeAlter(const std::string &sql, const std::vector<std::string> &params);
  
  private:
  std::shared_ptr<sql::Connection> conn_;
  SqlConnRAII* conn_RAII_ = nullptr;
};
