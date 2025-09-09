#include "protocol.h"

// 64位字节序转换函数
uint64_t htonll(uint64_t host64) {
  static const int num = 42;
  if (*reinterpret_cast<const char*>(&num) == 42) { // 小端序
    return ((uint64_t)htonl(host64 & 0xFFFFFFFF) << 32) | htonl(host64 >> 32);
  }
  else { // 大端序
    return host64;
  }
}

uint64_t ntohll(uint64_t net64) {
  return htonll(net64); // 转换逻辑相同
}

// 检查condition是否为真，不为真输出errmsg，并退出程序
void errCheck(bool condition, const char *errmsg) {
  if (condition) {
    perror(errmsg);
    exit(EXIT_FAILURE);
  }
}

// 将数据库检查回来的信息转换为用户信息结构
bool ConvertTouserInfo(std::vector<std::string>& info, UserInfo &ret) {
    if (info.empty()) {
        return false;
    }
    ret.header.type = ProtocolType::USERINFO_TYPE;
    ret.header.body_len = USERINFO_BODY_LEN;

    memcpy(ret.user, info[0].c_str(), info[0].size());
    memcpy(ret.pwd, info[1].c_str(), info[1].size());
    memcpy(ret.cipher, info[2].c_str(), info[2].size());
    memcpy(ret.is_vip, info[3].c_str(), info[3].size());
    memcpy(ret.capacity_sum, info[4].c_str(), info[4].size());
    memcpy(ret.used_capacity, info[5].c_str(), info[5].size());
    memcpy(ret.salt, info[6].c_str(), info[6].size());
    memcpy(ret.vip_date, info[7].c_str(), info[7].size());
    return true;
}

// 将一个保存文件信息的向量容器里的数据转换为FileInfo
bool VetToFileInfo(const std::vector<std::string>& vet, FileInfo &fileinfo) {
    if (vet.size() < 6) {
        return false;   // 如果长度对应不上，返回 false
    }

    try {
        // 头部
        fileinfo.header.type = ProtocolType::FILEINFO_TYPE;
        fileinfo.header.body_len = FILEINFO_BODY_LEN;
        
        // 将字符串转换为 uint64_t
        fileinfo.file_id = std::stoull(vet[0]);  

        // 复制文件名，确保不超过 100 字节
        strncpy(fileinfo.file_name, vet[1].c_str(), sizeof(fileinfo.file_name) - 1);
        fileinfo.file_name[sizeof(fileinfo.file_name) - 1] = '\0'; // 手动添加空终止符
        
        // 转换目录等级
        fileinfo.dir_grade = std::stoi(vet[2]);

        // 复制文件类型，确保不超过 10 字节
        strncpy(fileinfo.file_type, vet[3].c_str(), sizeof(fileinfo.file_type) - 1);
        fileinfo.file_type[sizeof(fileinfo.file_type) - 1] = '\0'; // 手动添加空终止符

        // 将字符串转换为 uint64_t
        fileinfo.file_size = std::stoull(vet[4]);

        // 将字符串转换为 uint64_t
        fileinfo.parent_dir = std::stoull(vet[5]);

        strncpy(fileinfo.file_date, vet[6].c_str(), sizeof(fileinfo.file_date) - 1);
        fileinfo.file_type[sizeof(fileinfo.file_type) - 1] = '\0'; // 手动添加空终止符
        
        return true;
    } catch (const std::exception &e) {
        std::cerr << "VetToFileInfo Error: " << e.what() << std::endl;
        return false;
    }
}

//生成40字节哈希码
std::string generateHash(const std::string &username, const std::string &password) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        throw std::runtime_error("无法创建 EVP_MD_CTX");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("初始化 EVP 摘要失败");
    }

    if (EVP_DigestUpdate(mdctx, username.data(), username.size()) != 1 ||
        EVP_DigestUpdate(mdctx, password.data(), password.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("更新 EVP 摘要失败");
    }

    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("最终化 EVP 摘要失败");
    }

    EVP_MD_CTX_free(mdctx);

    // 使用前40个字节
    std::string hash_hex;
    for (unsigned int i = 0; i < 20; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        hash_hex += buf;
    }

    return hash_hex;
}
