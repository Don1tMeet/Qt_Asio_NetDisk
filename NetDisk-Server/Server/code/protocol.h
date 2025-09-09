#pragma once

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <openssl/ssl.h>
#include <openssl/err.h>


#define IP "127.0.0.1"
#define PORT 8080
#define THREADNUM   10            // 线程数
#define MAXEVENTS   10            // events事件数量
#define USERSCOLLEN 8             // 数据库用户信息表列数
#define USERSCOLMAXSIZE 50        // 数据库用户信息表每列的最大长度
#define LOGPATH "./logfile"       // 日记保存路径
#define ROOTFILEPATH "rootfiles"  // 保存用户根文件路径

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

// 操作码
enum Code {
  UNKNOWN = 0,

  SIGNIN,             // 客户端要进行登陆操作
  SIGNUP,             // 客户端要进行注册操作

  PUTS,               // 客户端要进行上传操作
  PUTS_DATA,          // 客户端上传文件数据
  PUTS_FINISH,        // 客户端通知上传完成

  GETS,               // 客户端要进行下载操作
  GETS_DATA,          // 服务端发送文件数据
  GETS_FINISH,        // 服务端通知下载完成
  GETS_CONTROL,       // 客户端控制下载

  CD,                 // 客户端要读取文件信息
  MAKEDIR,            // 客户端要创建文件夹
  DELETEFILE,         // 客户端要删除文件

  CLIENTSHUT,         // 客户端关闭 

  PUTSCONTINUE,       // 断点上传

  GETCONTINUENO,      // 断点下载失败
};

// 状态码
enum Status {
  DEFAULT = 0,          // 默认状态
  SUCCESS,              // 成功
  FAILED,               // 失败

  NOT_VERIFY,           // 未验证

  NO_CAPACITY,          // 空间不足
  PUT_QUICK,            // 快传
  PUT_CONTINUE_FAILED,  // 断点续传失败
  GET_CONTINUE_FAILED,  // 端点下载失败

  FILE_NOT_EXIST,       // 文件不存在
};

// 控制操作
enum ControlAction {
  UNKNOWN_ACTION = 0,   // 未知操作
  PAUSE,                // 暂停
  RESUME,               // 继续
  CANCEL,               // 取消
};

// 协议头部结构体
#define PROTOCOLHEADER_LEN (2*sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t))
struct ProtocolHeader {
  uint16_t type{ 0 };       // 类型标识
  uint32_t body_len{ 0 };   // Body的长度（字节数）
  uint8_t version{ 0 };     // 协议版本（可选，用于兼容未来扩展）
  uint8_t reserved{ 0 };    // 预留字段（可选，用于对齐或未来扩展）
};

// 与服务端进行短任务交互的协议单元：如进行登陆、注册、删除、创建文件夹等功能
#define PDU_BODY_BASE_LEN (3*sizeof(uint32_t) + 140)
struct PDU {
  ProtocolHeader header;      // 头部（type=1）
  uint32_t code{ 0 };         // 状态码
  char user[20]{ 0 };         // 用户名，默认最长20，非中文
  char pwd[20]{ 0 };          // 密码，默认最长20，非中文
  char file_name[100]{ 0 };   // 可用,可不用。如删除文件，就使用。最长100字节
  uint32_t msg_len{ 0 };      // 备用空间长度，如有特别要求则使用，最长200字节
  char msg[200]{ 0 };         // 备用空间
};

// 用于对客户端请求的回复
#define PDURESPOND_BODY_BASE_LEN (4*sizeof(uint32_t))
struct PDURespond {
  ProtocolHeader header;      // 头部
  uint32_t code{ 0 };         // 操作码（用于标识登录，注册等操作）
  uint32_t status{ 0 };       // 状态码（用于表示回复状态，成功，失败等）
  uint32_t msg_amount{ 0 };   // 信息数量（信息可能由多个相同结构体组成，如文件数据）
  uint32_t msg_len{ 0 };      // 信息长度（用于发送额外信息，该长度为总长度）
  std::string msg{ "" };      // 额外信息
};

// 用于文件上传和下载的通信协议
#define TRANPDU_BODY_LEN (sizeof(uint32_t) + 3*sizeof(uint64_t) + 240)
struct TranPdu {
  ProtocolHeader header;              // 头部（type=2）
  uint32_t tran_pdu_code{ 0 };        // 操作码
  char user[20]{ 0 };                 // 用户名
  char pwd[20]{ 0 };                  // 密码
  char file_name[100]{ 0 };           // 文件名
  char file_md5[100]{ 0 };            // 文件MD5码
  uint64_t file_size{ 0 };            // 文件长度
  uint64_t sended_size{ 0 };          // 实现断点续传的长度
  uint64_t parent_dir_id{ 0 };        // 保存在哪个目录下的ID，为0则保存在根目录下
};

// 用于文件上传和下载文件数据的通信协议
#define TRANDATAPDU_BODY_BASE_LEN (6*sizeof(uint32_t) + sizeof(uint64_t))
struct TranDataPdu {
  ProtocolHeader header;
  uint32_t code{ 0 };             // 操作码
  uint32_t status{ 0 };           // 状态码，（暂不使用，由code代替）
  uint64_t file_offset{ 0 };      // 本次数据在文件中的偏移量
  uint32_t chunk_size{ 0 };       // 本次分片大小
  uint32_t total_chunks{ 0 };     // 总分片数（用于进度计算），（暂不使用）
  uint32_t chunk_index{ 0 };      // 当前分片索引（从0开始），（暂不使用）
  uint32_t check_sum{ 0 };        // 本次分片数据的校验和，（暂不使用）
  // !!!!!!!!!!!!!!!!!!!!! 使用string需要重新拷贝一次数据，后续使用ptr，和len !!!!!!!!!!!!!!!!!!!!!!!!!!!!
  std::string data{ "" };         // 文件数据
};

// 用于通知文件上传和下载完成的通信协议
#define TRANFINISHPDU_BODY_LEN (sizeof(uint32_t) + sizeof(uint64_t) + 100)
struct TranFinishPdu {
  ProtocolHeader header;
  uint32_t code{ 0 };         // 操作码
  uint64_t file_size{ 0 };    // 文件大小（用于二次校验）
  char file_md5[100]{ 0 };    // 文件MD5码
};

// 用于控制传输文件状态（主要是下载）的通信协议
#define TRANCONTROL_BODY_BASE_LEN (3*sizeof(uint32_t))
struct TranControlPdu {
  ProtocolHeader header;
  uint32_t code{ 0 };       // 操作码
  uint32_t action{ 0 };     // 控制行为
  uint32_t msg_len{ 0 };    // 控制信息长度
  std::string msg{ "" };    // 控制信息
};

// 服务器简版回复客户端包体，不用每次携带大量数据
#define RESPONDPACK_BODY_BASE_LEN (2 * sizeof(uint32_t))
struct RespondPack {
  ProtocolHeader header;      // 头部（type=3）
  std::uint32_t code = 0;     // 状态码
  std::uint32_t len = 0;      // 备用空间存有数据量长度
  char reserve[200]{ 0 };     // 备用空间
};

// 客户端信息结构体，用来保存从服务器接收的用户信息
#define USERINFO_BODY_LEN (USERSCOLLEN * USERSCOLMAXSIZE)
struct UserInfo {
  ProtocolHeader header;                      // 头部（type=4）
  char user[USERSCOLMAXSIZE] = { 0 };         // 用户名
  char pwd[USERSCOLMAXSIZE] = { 0 };          // 密码
  char cipher[USERSCOLMAXSIZE] = {  0 };      // 哈希密码，用来提供简易认证
  char is_vip[USERSCOLMAXSIZE] = { 0 };       // 是否VIP
  char capacity_sum[USERSCOLMAXSIZE] = { 0 }; // 云盘总空间大小
  char used_capacity[USERSCOLMAXSIZE] = { 0 };// 云盘已用空间大小
  char salt[USERSCOLMAXSIZE] = { 0 };         // 可用使用来提供多重认证
  char vip_date[USERSCOLMAXSIZE] = { 0 };     // 会员到期时间
};

// 文件信息体，即保存在数据库中的文件和文件夹
#define FILEINFO_BODY_LEN (sizeof(uint32_t) + 3*sizeof(uint64_t) + 210)
struct FileInfo {
  ProtocolHeader header;          // 头部（type=5）
  uint64_t file_id{ 0 };          // 文件在数据库的id
  char file_name[100]{ 0 };       // 文件名，最长100字节
  uint32_t dir_grade{ 0 };        // 目录等级，距离根目录距离，用来构建文件给客户端界面系统
  char file_type[10]{ 0 };        // 文件类型，如d为目录，f为文件，也可以拓展MP3等
  uint64_t file_size{ 0 };        // 文件大小
  uint64_t parent_dir{ 0 };       // 父文件夹的ID，指明文件保存在哪个文件夹下面
  char file_date[100]{ 0 };       // 文件的上传日期
  // 默认构造函数
  FileInfo() = default;
  // 带参数构造函数（用于方便初始化）
  FileInfo(std::uint64_t id, const char* name, int grade, const char* type, std::uint64_t size, std::uint64_t parent, const char *date)
    : file_id(id), dir_grade(grade), file_size(size), parent_dir(parent) {
    header.type = ProtocolType::FILEINFO_TYPE;
    header.body_len = FILEINFO_BODY_LEN;
    strncpy(file_name, name, sizeof(file_name) - 1);
    strncpy(file_type, type, sizeof(file_type) - 1);
    strncpy(file_date, date, sizeof(file_date) - 1);
  }
};

// 服务器信息包
#define SERVERINFOPACK_BODY_LEN (2*sizeof(uint32_t) + sizeof(uint64_t) + 52)
struct ServerInfoPack {
  ProtocolHeader header;      // 头部（type=6）
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

    size_t len = std::min(static_cast<size_t>(20), _ip.size());
    memcpy(ip, _ip.c_str(), len);
    ip[20] = 0;

    len = std::min(static_cast<size_t>(30), _name.size());
    memcpy(name, _name.c_str(), len);
    name[30] = 0;
  }
  ServerInfoPack()=default;
};

#pragma pack(push, 1)
#define SERVERSTATE_BODY_LEN (sizeof(uint32_t) + sizeof(uint64_t))
struct ServerState {
  ProtocolHeader header;              // 头部（type=7）
  std::uint32_t code = 0;             // 状态码，0为正常，1为关闭
  std::uint64_t cur_con_count = 0;    // 当前连接数
};
#pragma pack(pop)           //禁用内存对齐，方便网络传输

// 64位字节序转换函数
uint64_t htonll(uint64_t host64);

uint64_t ntohll(uint64_t net64);

// 检查condition是否为真，不为真输出errmsg，并退出程序
void errCheck(bool condition, const char *errmsg);

// 将数据库检查回来的信息转换为用户信息结构
bool ConvertTouserInfo(std::vector<std::string>& info, UserInfo &ret);

// 将一个保存文件信息的向量容器里的数据转换为FileInfo
bool VetToFileInfo(const std::vector<std::string>& vet, FileInfo &fileinfo);

//生成40字节哈希码
std::string generateHash(const std::string &username, const std::string &password);