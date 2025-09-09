#pragma once

#include "AbstractTool.h"


// 登录
class LoginTool : public AbstractTool {
 public:
  LoginTool(const PDU &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  PDU pdu_{ 0 };
  AbstractCon *conn_parent_{ nullptr };
};

// 注册
class SignTool : public AbstractTool {
 public:
  SignTool(const PDU &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  bool createDir();

 private:
  PDU pdu_{ 0 };
  AbstractCon *conn_parent_{ nullptr };
};

// 返回给客户端文件列表功能
class CdTool : public AbstractTool {
 public:
  CdTool(const PDU &pdu,AbstractCon *conn);
  int doingTask() override;

 private:
  PDU pdu_{ 0 };
  AbstractCon *conn_parent_{ nullptr };
};

// 创建文件夹
class CreateDirTool : public AbstractTool {
 public:
  CreateDirTool(const PDU &pdu, AbstractCon *conn);
  int doingTask() override;

private:
  PDU pdu_{ 0 };
  ClientCon *conn_{ nullptr };
};

// 删除文件或者文件夹
class DeleteTool:public AbstractTool{
 public:
  DeleteTool(const PDU &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  PDU pdu_{ 0 };
  ClientCon *conn_{ nullptr };
};
