#pragma once

#include <string>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

// 协议类型
enum ProtocolType {
  UNKNOWN_TYPE = 0,         // 未知类型
  PDU_TYPE,                 // PDU
  PDURESPOND_TYPE,          // PDURespond
  TRANPDU_TYPE,             // TranPdu
  TRANDATAPDU_TYPE,         // TranDataPdu
  TRANFINISHPDU_TYPE,       // TranFinishPdu
  TRANCONTROLPDU_TYPE,      // TranControlPdu
  RESPONDPACK_TYPE,         // RespondPack
  USERINFO_TYPE,            // UserInfo
  FILEINFO_TYPE,            // FileInfo
  SERVERINFOPACK_TYPE,      // ServerInfoPack
  SERVERSTATE_TYPE,         // ServerState
};

// 协议头部结构体
#define PROTOCOLHEADER_LEN (2*sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t))
struct ProtocolHeader {
  uint16_t type{ 0 };       // 类型标识
  uint32_t body_len{ 0 };   // Body的长度（字节数）
  uint8_t version{ 0 };     // 协议版本（可选，用于兼容未来扩展）
  uint8_t reserved{ 0 };    // 预留字段（可选，用于对齐或未来扩展）
};

// 服务器信息包
#define SERVERINFOPACK_BODY_LEN (2*sizeof(uint32_t) + sizeof(uint64_t) + 52)
struct ServerInfoPack {
  ProtocolHeader header;      // 头部
  char name[31]{ 0 };
  char ip[21]{ 0 };
  std::uint32_t sport{ 0 };
  std::uint32_t lport{ 0 };
  std::uint64_t cur_con_count{ 0 };

  ServerInfoPack(const std::string &_ip, const std::uint32_t &_sport, const std::uint32_t &_lport, const std::string &_name)
    :sport(_sport), lport(_lport)
  {
    header.type = ProtocolType::SERVERINFOPACK_TYPE;
    header.body_len = SERVERINFOPACK_BODY_LEN;

    size_t len = std::min(static_cast<size_t>(30), _name.size());
    memcpy(name, _name.c_str(), len);

    len = std::min(static_cast<size_t>(20), _ip.size());
    memcpy(ip, _ip.c_str(), len);
  }
  ServerInfoPack() = default;
};

#pragma pack(push, 1)
#define SERVERSTATE_BODY_LEN (sizeof(uint32_t) + sizeof(uint64_t))
struct ServerState {
  ProtocolHeader header;              // 头部
  std::uint32_t code = 0;             // 状态码，0为正常，1为关闭
  std::uint64_t cur_con_count = 0;    // 当前连接数
};
#pragma pack(pop)   //禁用内存对齐，方便网络传输