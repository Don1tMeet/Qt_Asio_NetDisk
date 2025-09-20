#pragma once

#include "protocol.h"


// !!!!!!!!!!!!!!!!!!!!!!!!!! 发送数据应该是线程安全的，后续更改 !!!!!!!!!!!!!!!!!!!!!!!!!!!
// send和recv的包装，主要用于网络发送和接收数据
class SRTool {
 public:
  

  size_t sendPDU(SSL *ssl, const PDU &pdu);     // 安全套接字发送PDU
  size_t sendPDURespond(SSL *ssl, const PDURespond &pdu);
  size_t sendTranDataPdu(SSL *ssl, const TranDataPdu &pdu);

  size_t sendUserInfo(SSL *ssl, const UserInfo &info);      //使用ssl发生客户信息

  bool sendFileInfo(SSL *ssl, std::vector<FileInfo> &vet);  //使用ssl把文件信息全部发送回客户端

  
};  
