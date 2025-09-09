#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <QtEndian>
#include <memory>
#include "protocol.h"

using buffer_shared_ptr = std::shared_ptr<char[]>;

// 序列化工具
class Serializer {
public:
    // 序列化与反序列化ProtocolHeader
    static buffer_shared_ptr serialize(const ProtocolHeader& header);
    static bool deserialize(const char* buf, size_t len, ProtocolHeader& header);

    // 序列化与反序列化PDU
    static buffer_shared_ptr serialize(const PDU& pdu);
    static bool deserialize(const char* buf, size_t len, PDU& pdu);

    // 序列化与反序列化PDURespond
    static buffer_shared_ptr serialize(const PDURespond& pdu);
    static bool deserialize(const char* buf, size_t len, PDURespond& pdu);

    // 序列化与反序列化TranPdu
    static buffer_shared_ptr serialize(const TranPdu& pdu);
    static bool deserialize(const char* buf, size_t len, TranPdu& pdu);

    // 序列化与反序列化TranDataPdu
    static buffer_shared_ptr serialize(const TranDataPdu& pdu);
    static bool deserialize(const char* buf, size_t len, TranDataPdu& pdu);

    // 序列化与反序列化TranFinishPdu
    static buffer_shared_ptr serialize(const TranFinishPdu& pdu);
    static bool deserialize(const char* buf, size_t len, TranFinishPdu& pdu);

    // 序列化与反序列化TranControlPdu
    static buffer_shared_ptr serialize(const TranControlPdu& pdu);
    static bool deserialize(const char* buf, size_t len, TranControlPdu& pdu);

    // 序列化与反序列化RespondPack
    static buffer_shared_ptr serialize(const RespondPack& pdu);
    static bool deserialize(const char* buf, size_t len, RespondPack& pdu);

    // 序列化与反序列化UserInfo
    static buffer_shared_ptr serialize(const UserInfo& pdu);
    static bool deserialize(const char* buf, size_t len, UserInfo& pdu);

    // 序列化与反序列化FileInfo
    static buffer_shared_ptr serialize(const FileInfo& pdu);
    static bool deserialize(const char* buf, size_t len, FileInfo& pdu);

    // 序列化与反序列化ServerInfoPack
    static buffer_shared_ptr serialize(const ServerInfoPack& pdu);
    static bool deserialize(const char* buf, size_t len, ServerInfoPack& pdu);

    // 将data的len字节写入ptr，返回写入的字节(len)
    static size_t writeData(char* ptr, const char* data, size_t len) {
        memcpy(ptr, data, len);
        return len;
    }

    // 16位字节序转换函数
    static uint16_t htons(uint16_t host32) {
        return qToBigEndian<uint16_t>(host32);
    }

    static uint16_t ntohs(uint16_t net32) {
        return qFromBigEndian<uint16_t>(net32);
    }

    // 32位字节序转换函数
    static uint32_t htonl(uint32_t host32) {
        return qToBigEndian<uint32_t>(host32);
    }

    static uint32_t ntohl(uint32_t net32) {
        return qFromBigEndian<uint32_t>(net32);
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


#endif // SERIALIZER_H
