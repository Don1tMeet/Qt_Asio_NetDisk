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

// 安全接收PDU，返回读取的长度
size_t SRTool::recvPDU(SSL *ssl, PDU &pdu) {

  size_t ret = SSL_read(ssl, &pdu.code, sizeof(std::uint32_t));    //依次发送code 直到 reserver
  ret += SSL_read(ssl, pdu.user, sizeof(pdu.user));
  ret += SSL_read(ssl, pdu.pwd, sizeof(pdu.pwd));
  ret += SSL_read(ssl, pdu.file_name, sizeof(pdu.file_name));
  ret += SSL_read(ssl, &pdu.msg_len, sizeof(std::uint32_t));

  if(pdu.msg_len > 0) { //如果使用了预留空间
    ret += SSL_read(ssl, pdu.msg, pdu.msg_len);
  }
  return ret;
}

// 安全接收传输PDU
size_t SRTool::recvTranPdu(SSL *ssl, TranPdu &pdu) {
  size_t ret = SSL_read(ssl, &pdu.tran_pdu_code, sizeof(int));
  ret += SSL_read(ssl, pdu.user, sizeof(pdu.user));
  ret += SSL_read(ssl, pdu.pwd, sizeof(pdu.pwd));
  ret += SSL_read(ssl, pdu.file_name, sizeof(pdu.file_name));
  ret += SSL_read(ssl, pdu.file_md5, sizeof(pdu.file_md5));
  ret += SSL_read(ssl, &pdu.file_size, sizeof(pdu.file_size));
  ret += SSL_read(ssl, &pdu.sended_size, sizeof(pdu.sended_size));
  ret += SSL_read(ssl, &pdu.parent_dir_id, sizeof(pdu.parent_dir_id));

  return ret;
}

// 安全发送简易版回复体，返回发送字节数
size_t SRTool::sendRespond(SSL *ssl, const RespondPack &respond) {
  // 序列化PDU（自动回收）
  auto buf = Serializer::serialize(respond);
  // 要发送的字节为Header大小+Body大小
  const size_t target_bytes = PROTOCOLHEADER_LEN + respond.header.body_len;
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

// 安全接收客户端简易响应
size_t SRTool::recvRespond(SSL *ssl, RespondPack &respond) {
  SSL_read(ssl, &respond.code, sizeof(respond.code));
  SSL_read(ssl, &respond.len, sizeof(respond.len));
  if(respond.len > 0) {
    SSL_read(ssl, respond.reserve, respond.len);
  }
  return respond.len + sizeof(respond.code) + sizeof(respond.len);
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