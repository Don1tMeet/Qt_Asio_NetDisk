#include "Serializer.h"
#include "BufferPool.h"

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

// 序列化PDU
buffer_shared_ptr Serializer::serialize(const PDU &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::PDU_TYPE) {  // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != PDU_BODY_BASE_LEN + pdu.msg_len) { // pdu大小产检
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
    uint32_t code = htonl(pdu.code);
    uint32_t msg_len = htonl(pdu.msg_len);
    // 写入Body
    ptr += writeData(ptr, (const char*)&code, sizeof(code));
    ptr += writeData(ptr, pdu.user, sizeof(pdu.user));
    ptr += writeData(ptr, pdu.pwd, sizeof(pdu.pwd));
    ptr += writeData(ptr, pdu.file_name, sizeof(pdu.file_name));
    ptr += writeData(ptr, (const char*)&msg_len, sizeof(msg_len));
    if (pdu.msg_len > 0) {
        ptr += writeData(ptr, pdu.msg, pdu.msg_len);
    }

    return buf; // 自动归还到池
}

// 反序列化PDU
bool Serializer::deserialize(const char *buf, size_t len, PDU &pdu) {
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
    if (pdu.header.type != ProtocolType::PDU_TYPE) {  // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData((char*)&pdu.code,    ptr, sizeof(pdu.code));
    ptr += writeData(pdu.user,            ptr, sizeof(pdu.user));
    ptr += writeData(pdu.pwd,             ptr, sizeof(pdu.pwd));
    ptr += writeData(pdu.file_name,       ptr, sizeof(pdu.file_name));
    ptr += writeData((char*)&pdu.msg_len, ptr, sizeof(pdu.msg_len));
    // 转换字节序
    pdu.code = ntohl(pdu.code);
    pdu.msg_len = ntohl(pdu.msg_len);
    if (pdu.msg_len > 0) {
        ptr += writeData(pdu.msg, ptr, pdu.msg_len);
    }

    return true;
}

// 序列化PDURespond
buffer_shared_ptr Serializer::serialize(const PDURespond &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::PDURESPOND_TYPE) { // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != PDURESPOND_BODY_BASE_LEN + pdu.msg_len) {  // pdu大小产检
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
    uint32_t code = htonl(pdu.code);
    uint32_t status = htonl(pdu.status);
    uint32_t msg_amount = htonl(pdu.msg_amount);
    uint32_t msg_len = htonl(pdu.msg_len);
    // 写入Body
    ptr += writeData(ptr, (const char*)&code, sizeof(code));
    ptr += writeData(ptr, (const char*)&status, sizeof(status));
    ptr += writeData(ptr, (const char*)&msg_amount, sizeof(msg_amount));
    ptr += writeData(ptr, (const char*)&msg_len, sizeof(msg_len));

    if (pdu.msg_len > 0) {
        ptr += writeData(ptr, pdu.msg.data(), pdu.msg_len);
    }

    return buf; // 自动归还到池
}

// 序列化PDURespond
bool Serializer::deserialize(const char *buf, size_t len, PDURespond &pdu) {
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
    if (pdu.header.type != ProtocolType::PDURESPOND_TYPE) { // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData((char*)&pdu.code,        ptr, sizeof(pdu.code));
    ptr += writeData((char*)&pdu.status,      ptr, sizeof(pdu.status));
    ptr += writeData((char*)&pdu.msg_amount,  ptr, sizeof(pdu.msg_amount));
    ptr += writeData((char*)&pdu.msg_len,     ptr, sizeof(pdu.msg_len));
    // 转换字节序
    pdu.code = ntohl(pdu.code);
    pdu.status = ntohl(pdu.status);
    pdu.msg_amount = ntohl(pdu.msg_amount);
    pdu.msg_len = ntohl(pdu.msg_len);
    if (pdu.msg_len > 0) {
        pdu.msg = std::string(ptr, pdu.msg_len);
    }

    return true;
}

// 序列化TranPdu
buffer_shared_ptr Serializer::serialize(const TranPdu &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::TRANPDU_TYPE) {  // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != TRANPDU_BODY_LEN) {  // pdu大小产检
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
    uint32_t code = htonl(pdu.tran_pdu_code); // 转换为网络字节序
    uint64_t file_size = htonll(pdu.file_size);
    uint64_t sended_size = htonll(pdu.sended_size);
    uint64_t parent_dir_id = htonll(pdu.parent_dir_id);
    // 写入Body
    ptr += writeData(ptr, (const char*)&code, sizeof(code));
    ptr += writeData(ptr, pdu.user, sizeof(pdu.user));
    ptr += writeData(ptr, pdu.pwd, sizeof(pdu.pwd));
    ptr += writeData(ptr, pdu.file_name, sizeof(pdu.file_name));
    ptr += writeData(ptr, pdu.file_md5, sizeof(pdu.file_md5));
    ptr += writeData(ptr, (const char*)&file_size, sizeof(file_size));
    ptr += writeData(ptr, (const char*)&sended_size, sizeof(sended_size));
    ptr += writeData(ptr, (const char*)&parent_dir_id, sizeof(parent_dir_id));

    return buf; // 自动归还到池
}

// 反序列化TranPdu
bool Serializer::deserialize(const char *buf, size_t len, TranPdu &pdu) {
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
    if (pdu.header.type != ProtocolType::TRANPDU_TYPE) {  // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData((char*)&pdu.tran_pdu_code, ptr, sizeof(pdu.tran_pdu_code));
    ptr += writeData(pdu.user,                  ptr, sizeof(pdu.user));
    ptr += writeData(pdu.pwd,                   ptr, sizeof(pdu.pwd));
    ptr += writeData(pdu.file_name,             ptr, sizeof(pdu.file_name));
    ptr += writeData(pdu.file_md5,              ptr, sizeof(pdu.file_md5));
    ptr += writeData((char*)&pdu.file_size,     ptr, sizeof(pdu.file_size));
    ptr += writeData((char*)&pdu.sended_size,   ptr, sizeof(pdu.sended_size));
    ptr += writeData((char*)&pdu.parent_dir_id, ptr, sizeof(pdu.parent_dir_id));
    // 转换字节序
    pdu.tran_pdu_code = ntohl(pdu.tran_pdu_code);
    pdu.file_size = ntohll(pdu.file_size);
    pdu.sended_size = ntohll(pdu.sended_size);
    pdu.parent_dir_id = ntohll(pdu.parent_dir_id);

    return true;
}

// 序列化TranDataPdu
buffer_shared_ptr Serializer::serialize(const TranDataPdu &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::TRANDATAPDU_TYPE) {  // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != TRANDATAPDU_BODY_BASE_LEN + pdu.chunk_size) {  // pdu大小产检
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
    uint32_t code = htonl(pdu.code);
    uint32_t status = htonl(pdu.status);
    uint64_t file_offset = htonll(pdu.file_offset);
    uint32_t chunk_size = htonl(pdu.chunk_size);
    uint32_t total_chunks = htonl(pdu.total_chunks);
    uint32_t chunk_index = htonl(pdu.chunk_index);
    uint32_t check_sum = htonl(pdu.check_sum);
    // 写入Body
    ptr += writeData(ptr, (const char*)&code, sizeof(code));
    ptr += writeData(ptr, (const char*)&status, sizeof(status));
    ptr += writeData(ptr, (const char*)&file_offset, sizeof(file_offset));
    ptr += writeData(ptr, (const char*)&chunk_size, sizeof(chunk_size));
    ptr += writeData(ptr, (const char*)&total_chunks, sizeof(total_chunks));
    ptr += writeData(ptr, (const char*)&chunk_index, sizeof(chunk_index));
    ptr += writeData(ptr, (const char*)&check_sum, sizeof(check_sum));
    if (pdu.chunk_size > 0 && pdu.chunk_size <= pdu.data.size()) {
        ptr += writeData(ptr, pdu.data.data(), pdu.chunk_size); // 写入data
    }

    return buf; // 自动归还到池
}

// 反序列化TranDataPdu
bool Serializer::deserialize(const char *buf, size_t len, TranDataPdu &pdu) {
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
    if (pdu.header.type != ProtocolType::TRANDATAPDU_TYPE) {  // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData((char*)&pdu.code,          ptr, sizeof(pdu.code));
    ptr += writeData((char*)&pdu.status,        ptr, sizeof(pdu.status));
    ptr += writeData((char*)&pdu.file_offset,   ptr, sizeof(pdu.file_offset));
    ptr += writeData((char*)&pdu.chunk_size,    ptr, sizeof(pdu.chunk_size));
    ptr += writeData((char*)&pdu.total_chunks,  ptr, sizeof(pdu.total_chunks));
    ptr += writeData((char*)&pdu.chunk_index,   ptr, sizeof(pdu.chunk_index));
    ptr += writeData((char*)&pdu.check_sum,     ptr, sizeof(pdu.check_sum));
    // 转换字节序
    pdu.code = ntohl(pdu.code);
    pdu.status = ntohl(pdu.status);
    pdu.file_offset = ntohll(pdu.file_offset);
    pdu.chunk_size = ntohl(pdu.chunk_size);
    pdu.total_chunks = ntohl(pdu.total_chunks);
    pdu.chunk_index = ntohl(pdu.chunk_index);
    pdu.check_sum = ntohl(pdu.check_sum);

    // 解析data
    if (pdu.header.body_len != TRANDATAPDU_BODY_BASE_LEN + pdu.chunk_size) {  // 判断body_len是否正确
        return false;
    }
    if (pdu.chunk_size > 0) {
        pdu.data.assign(ptr, pdu.chunk_size);
    }

    return true;
}

// 序列化TranFinishPdu
buffer_shared_ptr Serializer::serialize(const TranFinishPdu &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::TRANFINISHPDU_TYPE) {  // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != TRANFINISHPDU_BODY_LEN) {  // pdu大小产检
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
    uint32_t code = htonl(pdu.code);  // 转换为网络字节序
    uint64_t file_size = htonll(pdu.file_size);
    // 写入Body
    ptr += writeData(ptr, (const char*)&code, sizeof(code));
    ptr += writeData(ptr, (const char*)&file_size, sizeof(file_size));
    ptr += writeData(ptr, pdu.file_md5, sizeof(pdu.file_md5));

    return buf; // 自动归还到池
}

// 反序列化TranFinishPdu
bool Serializer::deserialize(const char *buf, size_t len, TranFinishPdu &pdu) {
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
    if (pdu.header.type != ProtocolType::TRANFINISHPDU_TYPE) {  // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData((char*)&pdu.code,      ptr, sizeof(pdu.code));
    ptr += writeData((char*)&pdu.file_size, ptr, sizeof(pdu.file_size));
    ptr += writeData(pdu.file_md5,          ptr, sizeof(pdu.file_md5));
    // 转换字节序
    pdu.code = ntohl(pdu.code);
    pdu.file_size = ntohll(pdu.file_size);

    return true;
}

// 序列化TranControlPdu
buffer_shared_ptr Serializer::serialize(const TranControlPdu &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::TRANCONTROLPDU_TYPE) {   // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != TRANCONTROL_BODY_BASE_LEN + pdu.msg_len) {   // pdu大小产检
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
    uint32_t code = htonl(pdu.code);  // 转换为网络字节序
    uint32_t action = htonl(pdu.action);
    uint32_t msg_len = htonl(pdu.msg_len);
    // 写入Body
    ptr += writeData(ptr, (const char*)&code, sizeof(code));
    ptr += writeData(ptr, (const char*)&action, sizeof(action));
    ptr += writeData(ptr, (const char*)&msg_len, sizeof(msg_len));
    if (pdu.msg_len > 0) {
        ptr += writeData(ptr, pdu.msg.data(), pdu.msg_len);
    }

    return buf; // 自动归还到池
}

// 反序列化TranControlPdu
bool Serializer::deserialize(const char *buf, size_t len, TranControlPdu &pdu) {
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
    if (pdu.header.type != ProtocolType::TRANCONTROLPDU_TYPE) {  // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData((char*)&pdu.code,    ptr, sizeof(pdu.code));
    ptr += writeData((char*)&pdu.action,  ptr, sizeof(pdu.action));
    ptr += writeData((char*)&pdu.msg_len, ptr, sizeof(pdu.msg_len));
    // 转换字节序
    pdu.code = ntohl(pdu.code);
    pdu.action = ntohl(pdu.action);
    pdu.msg_len = ntohl(pdu.msg_len);
    if (pdu.msg_len > 0) {
        pdu.msg.assign(ptr, pdu.msg_len);
    }

    return true;
}

// 序列化RespondPack
buffer_shared_ptr Serializer::serialize(const RespondPack &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::RESPONDPACK_TYPE) {  // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != RESPONDPACK_BODY_BASE_LEN + pdu.len) { // pdu大小产检
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
    uint32_t code = htonl(pdu.code);  // 转换为网络字节序
    uint32_t len = htonl(pdu.len);
    // 写入Body
    ptr += writeData(ptr, (const char*)&code, sizeof(code));
    ptr += writeData(ptr, (const char*)&len, sizeof(len));
    if (pdu.len > 0) {
        ptr += writeData(ptr, pdu.reserve, pdu.len);
    }

    return buf; // 自动归还到池
}

// 反序列化RespondPack
bool Serializer::deserialize(const char *buf, size_t len, RespondPack &pdu) {
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
    if (pdu.header.type != ProtocolType::RESPONDPACK_TYPE) {  // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData((char*)&pdu.code, ptr, sizeof(pdu.code));
    ptr += writeData((char*)&pdu.len,  ptr, sizeof(pdu.len));
    // 转换字节序
    pdu.code = ntohl(pdu.code);
    pdu.len = ntohl(pdu.len);
    if (pdu.len > 0) {
        ptr += writeData(pdu.reserve, ptr, pdu.len);
    }

    return true;
}

// 序列化UserInfo
buffer_shared_ptr Serializer::serialize(const UserInfo &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::USERINFO_TYPE) { // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != USERINFO_BODY_LEN) { // pdu大小产检
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
    // 写入Body
    ptr += writeData(ptr, pdu.user, sizeof(pdu.user));
    ptr += writeData(ptr, pdu.pwd, sizeof(pdu.pwd));
    ptr += writeData(ptr, pdu.cipher, sizeof(pdu.cipher));
    ptr += writeData(ptr, pdu.is_vip, sizeof(pdu.is_vip));
    ptr += writeData(ptr, pdu.capacity_sum, sizeof(pdu.capacity_sum));
    ptr += writeData(ptr, pdu.used_capacity, sizeof(pdu.used_capacity));
    ptr += writeData(ptr, pdu.salt, sizeof(pdu.salt));
    ptr += writeData(ptr, pdu.vip_date, sizeof(pdu.vip_date));

    return buf; // 自动归还到池
}

// 反序列化UserInfo
bool Serializer::deserialize(const char *buf, size_t len, UserInfo &pdu) {
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
    if (pdu.header.type != ProtocolType::USERINFO_TYPE) { // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData(pdu.user,          ptr, sizeof(pdu.user));
    ptr += writeData(pdu.pwd,           ptr, sizeof(pdu.pwd));
    ptr += writeData(pdu.cipher,        ptr, sizeof(pdu.cipher));
    ptr += writeData(pdu.is_vip,        ptr, sizeof(pdu.is_vip));
    ptr += writeData(pdu.capacity_sum,  ptr, sizeof(pdu.capacity_sum));
    ptr += writeData(pdu.used_capacity, ptr, sizeof(pdu.used_capacity));
    ptr += writeData(pdu.salt,          ptr, sizeof(pdu.salt));
    ptr += writeData(pdu.vip_date,      ptr, sizeof(pdu.vip_date));

    return true;
}

// 序列化FileInfo
buffer_shared_ptr Serializer::serialize(const FileInfo &pdu) {
    const size_t total_len = PROTOCOLHEADER_LEN + pdu.header.body_len;  // 总长度，头部+body长度
    // 检查缓冲区大小是否足够，不足抛出错误（或扩容）
    if (total_len > BufferPool::getInstance().getBufferSize()) {
        throw std::runtime_error("缓冲区大小不足, 无法序列化PDU");
    }
    if (pdu.header.type != ProtocolType::FILEINFO_TYPE) { // 检查类型
        throw std::runtime_error("发送通信协议类型错误, 不是预期类型");
    }
    if (pdu.header.body_len != FILEINFO_BODY_LEN) { // pdu大小产检
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
    uint64_t file_id = htonll(pdu.file_id);
    uint32_t dir_grade = htonl(pdu.dir_grade);
    uint64_t file_size = htonll(pdu.file_size);
    uint64_t parent_dir = htonll(pdu.parent_dir);
    // 写入Body
    ptr += writeData(ptr, (const char*)&file_id, sizeof(file_id));
    ptr += writeData(ptr, pdu.file_name, sizeof(pdu.file_name));
    ptr += writeData(ptr, (const char*)&dir_grade, sizeof(dir_grade));
    ptr += writeData(ptr, pdu.file_type, sizeof(pdu.file_type));
    ptr += writeData(ptr, (const char*)&file_size, sizeof(file_size));
    ptr += writeData(ptr, (const char*)&parent_dir, sizeof(parent_dir));
    ptr += writeData(ptr, pdu.file_date, sizeof(pdu.file_date));

    return buf; // 自动归还到池
}

// 反序列化FileInfo
bool Serializer::deserialize(const char *buf, size_t len, FileInfo &pdu) {
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
    if (pdu.header.type != ProtocolType::FILEINFO_TYPE) { // 验证类型
        return false;
    }
    if (len < PROTOCOLHEADER_LEN + pdu.header.body_len) { // 判断Body是否完整
        return false;
    }

    // 解析Body
    ptr += writeData((char*)&pdu.file_id,     ptr, sizeof(pdu.file_id));
    ptr += writeData(pdu.file_name,           ptr, sizeof(pdu.file_name));
    ptr += writeData((char*)&pdu.dir_grade,   ptr, sizeof(pdu.dir_grade));
    ptr += writeData(pdu.file_type,           ptr, sizeof(pdu.file_type));
    ptr += writeData((char*)&pdu.file_size,   ptr, sizeof(pdu.file_size));
    ptr += writeData((char*)&pdu.parent_dir,  ptr, sizeof(pdu.parent_dir));
    ptr += writeData(pdu.file_date,           ptr, sizeof(pdu.file_date));
    // 转换字节序
    pdu.file_id = ntohll(pdu.file_id);
    pdu.dir_grade = ntohl(pdu.dir_grade);
    pdu.file_size = ntohll(pdu.file_size);
    pdu.parent_dir = ntohll(pdu.parent_dir);

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
