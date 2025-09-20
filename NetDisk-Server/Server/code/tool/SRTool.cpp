#include "SRTool.h"
#include "BufferPool.h"
#include "Serializer.h"


// 安全发送PDU，返回发送的字节数
size_t SRTool::sendPDU(SSL *ssl, const PDU &pdu) {
  // 序列化PDU（自动回收）
  auto buf = Serializer::serialize(pdu);
  // 要发送的字节为Header大小+Body大小
  const size_t target_bytes = PROTOCOLHEADER_LEN + pdu.header.body_len;
  size_t sended_bytes = 0;  // 已发送大小
  // 发送header
  while (sended_bytes < target_bytes) {
    size_t ret = SSL_write(ssl, buf.get() + sended_bytes, target_bytes - sended_bytes);
    if (ret <= 0) {
      int err = SSL_get_error(ssl, ret);  // 获取错误信息
      if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {  // 暂时无法发送，稍后重试
        continue;
      }
      else {  // 其他错误，终止发送
        perror("SSL_write failed");
        break;
      }
    }
    sended_bytes += ret;
  } 
  return sended_bytes;
}

// 安全发送PDU回复
size_t SRTool::sendPDURespond(SSL *ssl, const PDURespond &pdu) {
  // 序列化PDU（自动回收）
  auto buf = Serializer::serialize(pdu);
  // 要发送的字节为Header大小+Body大小
  const size_t target_bytes = PROTOCOLHEADER_LEN + pdu.header.body_len;
  size_t sended_bytes = 0;  // 已发送大小
  // 发送header
  while (sended_bytes < target_bytes) {
    size_t ret = SSL_write(ssl, buf.get() + sended_bytes, target_bytes - sended_bytes);
    if (ret <= 0) {
      int err = SSL_get_error(ssl, ret);  // 获取错误信息
      if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {  // 暂时无法发送，稍后重试
        continue;
      }
      else {  // 其他错误，终止发送
        perror("SSL_write failed");
        break;
      }
    }
    sended_bytes += ret;
  }
  return sended_bytes;
}

size_t SRTool::sendTranDataPdu(SSL *ssl, const TranDataPdu &pdu) {
  // 序列化PDU（自动回收）
  auto buf = Serializer::serialize(pdu);
  // 要发送的字节为Header大小+Body大小
  const size_t target_bytes = PROTOCOLHEADER_LEN + pdu.header.body_len;
  size_t sended_bytes = 0;  // 已发送大小
  // 发送header
  while (sended_bytes < target_bytes) {
    size_t ret = SSL_write(ssl, buf.get() + sended_bytes, target_bytes - sended_bytes);
    if (ret <= 0) {
      int err = SSL_get_error(ssl, ret);  // 获取错误信息
      if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {  // 暂时无法发送，稍后重试
        continue;
      }
      else {  // 其他错误，终止发送
        perror("SSL_write failed");
        break;
      }
    }
    sended_bytes += ret;
  } 
  return sended_bytes;
}

// ssl发送客户端信息
size_t SRTool::sendUserInfo(SSL *ssl, const UserInfo &info) {
  // 序列化PDU（自动回收）
  auto buf = Serializer::serialize(info);
  // 要发送的字节为Header大小+Body大小
  const size_t target_bytes = PROTOCOLHEADER_LEN + info.header.body_len;
  size_t sended_bytes = 0;  // 已发送大小
  // 发送header
  while (sended_bytes < target_bytes) {
    size_t ret = SSL_write(ssl, buf.get() + sended_bytes, target_bytes - sended_bytes);
    if (ret <= 0) {
      int err = SSL_get_error(ssl, ret);  // 获取错误信息
      if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {  // 暂时无法发送，稍后重试
        continue;
      }
      else {  // 其他错误，终止发送
        perror("SSL_write failed");
        break;
      }
    }
    sended_bytes += ret;
  } 
  return sended_bytes; 
}

// 将一个存在全部文件信息的向量发送回客户端
bool SRTool::sendFileInfo(SSL *ssl, std::vector<FileInfo> &vet) {
  // 遍历文件信息结构体的向量
  for (FileInfo &file_info : vet) {
    // 设置协议头
    file_info.header.type = ProtocolType::FILEINFO_TYPE;
    file_info.header.body_len = FILEINFO_BODY_LEN;
    // 序列化PDU（自动回收）
    auto buf = Serializer::serialize(file_info);
    // 要发送的字节为Header大小+Body大小
    const size_t target_bytes = PROTOCOLHEADER_LEN + file_info.header.body_len;
    size_t sended_bytes = 0;  // 已发送大小
    // 发送header
    while (sended_bytes < target_bytes) {
      size_t ret = SSL_write(ssl, buf.get() + sended_bytes, target_bytes - sended_bytes);
      if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);  // 获取错误信息
        if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {  // 暂时无法发送，稍后重试
          continue;
        }
        else {  // 其他错误，终止发送
          perror("SSL_write failed");
          return false;
        }
      }
      sended_bytes += ret;
    } 
  }
  
  return true;  // 如果所有数据发送成功，返回 true
}