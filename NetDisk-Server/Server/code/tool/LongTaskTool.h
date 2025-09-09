#pragma once


#include "AbstractTool.h"
#include "AbstractCon.h"

// 负责上传任务
class PutsTool : public AbstractTool {
 public:
  PutsTool(AbstractCon *conn);
  PutsTool(const TranPdu &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  bool firstCheck(UpDownCon *conn);   // 首次连接认证
  UDtask createTask(PDURespond &respond, MyDB &db);  // 生成任务结构体

 private:
  TranPdu pdu_{ 0 };
  AbstractCon *conn_parent_{ nullptr };
};

// 负责上传文件数据任务
class PutsDataTool : public AbstractTool {
 public:
  PutsDataTool(AbstractCon* conn);
  PutsDataTool(const TranDataPdu &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  void recvFileData(UpDownCon *conn);

 private:
  TranDataPdu pdu_{ {0} };
  UpDownCon *conn_{ nullptr };
};

// 负责上传文件完成任务
class PutsFinishTool : public AbstractTool {
 public:
  PutsFinishTool(AbstractCon* conn);
  PutsFinishTool(const TranFinishPdu &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  TranFinishPdu pdu_{ 0 };
  UpDownCon *conn_{ nullptr };
};

// 负责下载任务
class GetsTool : public AbstractTool {
 public:
  GetsTool(AbstractCon *conn);
  GetsTool(const TranPdu &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  bool firstCheck();      //首次连接认证
  bool createTask(PDURespond &respond, UDtask &task, MyDB &db);

 private:
  TranPdu pdu_{ 0 };
  UpDownCon *conn_{ nullptr };
};

// 负责下载文件数据任务
class GetsDataTool : public AbstractTool {
 public:
  GetsDataTool(AbstractCon* conn);
  GetsDataTool(const TranDataPdu &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  void sendFileData();

 private:
  TranDataPdu pdu_{ {0} };
  UpDownCon *conn_{ nullptr };
};

// 负责下载文件完成任务
class GetsFinishTool : public AbstractTool {
 public:
  GetsFinishTool(AbstractCon* conn);
  GetsFinishTool(const TranFinishPdu &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  TranFinishPdu pdu_{ 0 };
  UpDownCon *conn_{ nullptr };
};

// 负责下载文件控制任务
class GetsControlTool : public AbstractTool {
 public:
  GetsControlTool(AbstractCon* conn);
  GetsControlTool(const TranControlPdu &pdu, AbstractCon *conn);
  int doingTask() override;

 private:
  TranControlPdu pdu_{ {0} };
  UpDownCon *conn_{ nullptr };
};
