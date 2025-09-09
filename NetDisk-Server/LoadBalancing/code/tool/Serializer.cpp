#include "Serializer.h"

// 序列化ProtocolHeader
buffer_shared_ptr Serializer::serialize(const ProtocolHeader& header) {
  const size_t total_len = PROTOCOLHEADER_LEN;  // 总长度
  // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
  if (total_len > BufferPool::getInstance().getBufferSize()) {
    throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
  }
  // 获取缓冲区
  auto buf = BufferPool::getInstance().acquire();
  char* ptr = buf.get();

  // 序列化Header
  uint16_t type = htons(header.type);   // 转换为网络字节序
  uint32_t body_len = htonl(header.body_len);
  // 写入Header
  ptr += writeData(ptr, (const char*)&type, sizeof(type));
  ptr += writeData(ptr, (const char*)&body_len, sizeof(body_len));
  ptr += writeData(ptr, (const char*)&header.version, sizeof(header.version));
  ptr += writeData(ptr, (const char*)&header.reserved, sizeof(header.reserved));

  return buf;
}

// 反序列化ProtocolHeader
bool Serializer::deserialize(const char* buf, size_t len, ProtocolHeader& header) {
  if (len < PROTOCOLHEADER_LEN) {
    return false;
  }

  const char* ptr = buf;
  // 解析Header
  ptr += writeData((char*)&header.type,      ptr, sizeof(header.type));
  ptr += writeData((char*)&header.body_len,  ptr, sizeof(header.body_len));
  ptr += writeData((char*)&header.version,   ptr, sizeof(header.version));
  ptr += writeData((char*)&header.reserved,  ptr, sizeof(header.reserved));
  // 转换字节序
  header.type = ntohs(header.type);
  header.body_len = ntohl(header.body_len);

  return true;
}

// 序列化ServerInfoPack
buffer_shared_ptr Serializer::serialize(const ServerInfoPack &pdu) {
  const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
  // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
  if (total_len > BufferPool::getInstance().getBufferSize()) {
    throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
  }
  if (pdu.header.type != ProtocolType::SERVERINFOPACK_TYPE) { // 检查类型
    throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
  }
  if (pdu.header.body_len != SERVERINFOPACK_BODY_LEN) { // pdu大小产检
    throw std::runtime_error("发送通信协议类型错误, body_len不是预期大小");
  }
  // 获取缓冲区
  auto buf = BufferPool::getInstance().acquire();
  char* ptr = buf.get();

  // 序列化Header
  ProtocolHeader header = pdu.header;
  header.type = htons(header.type);   // 转换为网络字节序
  header.body_len = htonl(header.body_len);
  // 写入Header
  ptr += writeData(ptr, (const char*)&header.type, sizeof(header.type));
  ptr += writeData(ptr, (const char*)&header.body_len, sizeof(header.body_len));
  ptr += writeData(ptr, (const char*)&header.version, sizeof(header.version));
  ptr += writeData(ptr, (const char*)&header.reserved, sizeof(header.reserved));

  // 序列化Body
  std::uint32_t sport = htonl(pdu.sport);
  std::uint32_t lport = htonl(pdu.lport);
  std::uint64_t cur_con_count = htonll(pdu.cur_con_count);
  // 写入Body
  ptr += writeData(ptr, pdu.name, sizeof(pdu.name));
  ptr += writeData(ptr, pdu.ip, sizeof(pdu.ip));
  ptr += writeData(ptr, (const char*)&sport, sizeof(sport));
  ptr += writeData(ptr, (const char*)&lport, sizeof(lport));
  ptr += writeData(ptr, (const char*)&cur_con_count, sizeof(cur_con_count));

  return buf; // 自动归还到池
}

// 反序列化ServerInfoPack
bool Serializer::deserialize(const char *buf, size_t len, ServerInfoPack &pdu) {
  if (len < PROTOCOLHEADER_LEN) {
    return false;
  }

  const char* ptr = buf;
  // 解析Header
  ptr += writeData((char*)&pdu.header.type,      ptr, sizeof(pdu.header.type));
  ptr += writeData((char*)&pdu.header.body_len,  ptr, sizeof(pdu.header.body_len));
  ptr += writeData((char*)&pdu.header.version,   ptr, sizeof(pdu.header.version));
  ptr += writeData((char*)&pdu.header.reserved,  ptr, sizeof(pdu.header.reserved));
  // 转换字节序
  pdu.header.type = ntohs(pdu.header.type);
  pdu.header.body_len = ntohl(pdu.header.body_len);
  if (pdu.header.type != ProtocolType::SERVERINFOPACK_TYPE) { // 验证类型
    return false;
  }
  if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
    return false;
  }
  
  // 解析Body
  ptr += writeData(pdu.name,                  ptr, sizeof(pdu.name));
  ptr += writeData(pdu.ip,                    ptr, sizeof(pdu.ip));
  ptr += writeData((char*)&pdu.sport,         ptr, sizeof(pdu.sport));
  ptr += writeData((char*)&pdu.lport,         ptr, sizeof(pdu.lport));
  ptr += writeData((char*)&pdu.cur_con_count, ptr, sizeof(pdu.cur_con_count));
  // 转换字节序
  pdu.sport = ntohl(pdu.sport);
  pdu.lport = ntohl(pdu.lport);
  pdu.cur_con_count = ntohll(pdu.cur_con_count);

  return true;
}

// 序列化ServerState
buffer_shared_ptr Serializer::serialize(const ServerState &pdu) {
  const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
  // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
  if (total_len > BufferPool::getInstance().getBufferSize()) {
    throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
  }
  if (pdu.header.type != ProtocolType::SERVERSTATE_TYPE) {  // 检查类型
    throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
  }
  if (pdu.header.body_len != SERVERSTATE_BODY_LEN) {  // pdu大小产检
    throw std::runtime_error("发送通信协议类型错误, body_len不是预期大小");
  }
  // 获取缓冲区
  auto buf = BufferPool::getInstance().acquire();
  char* ptr = buf.get();

  // 序列化Header
  ProtocolHeader header = pdu.header;
  header.type = htons(header.type);   // 转换为网络字节序
  header.body_len = htonl(header.body_len);
  // 写入Header
  ptr += writeData(ptr, (const char*)&header.type, sizeof(header.type));
  ptr += writeData(ptr, (const char*)&header.body_len, sizeof(header.body_len));
  ptr += writeData(ptr, (const char*)&header.version, sizeof(header.version));
  ptr += writeData(ptr, (const char*)&header.reserved, sizeof(header.reserved));

  // 序列化Body
  std::uint32_t code = htonl(pdu.code);
  std::uint64_t cur_con_count = htonll(pdu.cur_con_count);
  // 写入Body
  ptr += writeData(ptr, (const char*)&code, sizeof(code));
  ptr += writeData(ptr, (const char*)&cur_con_count, sizeof(cur_con_count));

  return buf; // 自动归还到池
}

// 反序列化ServerState
bool Serializer::deserialize(const char *buf, size_t len, ServerState &pdu) {
  if (len < PROTOCOLHEADER_LEN) {
    return false;
  }

  const char* ptr = buf;
  // 解析Header
  ptr += writeData((char*)&pdu.header.type,     ptr, sizeof(pdu.header.type));
  ptr += writeData((char*)&pdu.header.body_len, ptr, sizeof(pdu.header.body_len));
  ptr += writeData((char*)&pdu.header.version,  ptr, sizeof(pdu.header.version));
  ptr += writeData((char*)&pdu.header.reserved, ptr, sizeof(pdu.header.reserved));
  // 转换字节序
  pdu.header.type = ntohs(pdu.header.type);
  pdu.header.body_len = ntohl(pdu.header.body_len);
  if (pdu.header.type != ProtocolType::SERVERSTATE_TYPE) { // 验证类型
    return false;
  }
  if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
    return false;
  }
  
  // 解析Body
  ptr += writeData((char*)&pdu.code,          ptr, sizeof(pdu.code));
  ptr += writeData((char*)&pdu.cur_con_count, ptr, sizeof(pdu.cur_con_count));
  // 转换字节序
  pdu.code = ntohl(pdu.code);
  pdu.cur_con_count = ntohll(pdu.cur_con_count);

  return true;
}
