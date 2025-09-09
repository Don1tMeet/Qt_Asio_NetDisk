#pragma once

#include <memory>
#include "BufferPool.h"
#include "protocol.h"

// 序列化工具
class Serializer {
 public:
  // 序列化与反序列化ProtocolHeader
  static buffer_shared_ptr serialize(const ProtocolHeader& header);
  static bool deserialize(const char* buf, size_t len, ProtocolHeader& header);

  // 序列化与反序列化ServerInfoPack
  static buffer_shared_ptr serialize(const ServerInfoPack& pdu);
  static bool deserialize(const char* buf, size_t len, ServerInfoPack& pdu);

  // 序列化与反序列化ServerState
  static buffer_shared_ptr serialize(const ServerState& pdu);
  static bool deserialize(const char* buf, size_t len, ServerState& pdu);

  // 将data的len字节写入ptr，返回写入的字节(len)
  static size_t writeData(char* ptr, const char* data, size_t len) {
    memcpy(ptr, data, len);
    return len;
  }

  // 64位字节序转换函数
  static uint64_t htonll(uint64_t host64) {
    static const int num = 42;
    if (*reinterpret_cast<const char*>(&num) == 42) { // 小端序
      return ((uint64_t)htonl(host64 & 0xFFFFFFFF) << 32) | htonl(host64 >> 32);
    }
    else { // 大端序
      return host64;
    }
  }

  static uint64_t ntohll(uint64_t net64) {
    return htonll(net64); // 转换逻辑相同
  }
};
