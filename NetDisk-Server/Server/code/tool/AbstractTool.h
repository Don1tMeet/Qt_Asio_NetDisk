#pragma once

#include "MyDB.h"
#include "ClientCon.h"
#include "UpDownCon.h"
#include "SRTool.h"

class AbstractTool {
 public:
  AbstractTool() = default;
  virtual ~AbstractTool() = default;
  virtual int doingTask() = 0;

 protected:
  SRTool sr_tool_;
  std::string getSuffix(const std::string &file_name);    // 获取文件后缀名
};
