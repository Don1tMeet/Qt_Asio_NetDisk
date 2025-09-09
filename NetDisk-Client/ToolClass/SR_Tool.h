#pragma once

#include <QObject>
#include <QFile>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <atomic>
#include "Serializer.h"
#include "protocol.h"

// 普通套接字共享指针
using socket_ptr = std::shared_ptr<boost::asio::ip::tcp::socket>;
// 安全套接字共享指针
using sslsock_ptr = std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>;


// 用于简易处理与服务端交互的工具，网络收发使用Asio库实现.可使用同步发送和异步发送
class SR_Tool : public QObject, public std::enable_shared_from_this<SR_Tool>
{
    Q_OBJECT

public:
    SR_Tool(const std::string &ep_addr, const int &ep_port, QObject* parent = nullptr);
    ~SR_Tool();

public:
    // 同步操作，传递一个boost::system::error_code，结果放在ec
    void connect(boost::system::error_code& ec);                        // 同步连接
    void send(char* buf, size_t len, boost::system::error_code& ec);    // 同步发送
    void recv(char* buf, size_t len, boost::system::error_code& ec);    // 同步接收
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket>* getSSL();   // 返回ssl指针
    boost::asio::ip::tcp::endpoint getEndpoint();                       // 返回对端对象

    bool sendPDU(const PDU& pdu);               // 发送PDU到服务器
    bool recvRespond(RespondPack& res);         // 同步接收一个回复
    bool recvFileInfo(FileInfo& file_info);     // 接收一条文件信息

public:
    // 异步操作，异步事件完成会触发相关信号。连接信号，接收异步结果。也可以传递回调函数，在异步事件就绪时候，调用回调函数。调用异步任务后，需要SR_run()启动异步循环
    void asyncConnect();                            // 异步连接服务器
    void asyncConnect(std::function<void()> fun);   // 传递函数对象，异步连接

    void asyncSend(buffer_shared_ptr buf, size_t len);          // 异步发送数据
    void asyncSend(buffer_shared_ptr buf, size_t len, std::function<void()> fun);    // 传递函数对象，异步发送

    void asyncSendFileDataStart(QFile *file, std::set<uint32_t> &unacked_id, TranPdu &tran_pdu,
                                std::shared_ptr<std::atomic<std::uint32_t>> control,
                                std::shared_ptr<std::condition_variable> cv, std::shared_ptr<std::mutex> mutex);
    void asyncSendFileDataContinue(QFile *file, uint32_t chunk_size, uint64_t file_size, uint32_t total_chunks, uint32_t last_chunk_size, uint32_t cur_chunk_id,
                                   std::shared_ptr<std::atomic<std::uint32_t>> control,
                                   std::shared_ptr<std::condition_variable> cv, std::shared_ptr<std::mutex> mutex);

    void asyncRecvProtocol(buffer_shared_ptr buf, size_t header_len = PROTOCOLHEADER_LEN);    // 异步接收ProtocolHeader
    void asyncRecvBody(buffer_shared_ptr buf, size_t body_len, size_t header_len = PROTOCOLHEADER_LEN); // 异步接收Body（在buf中已接受header的基础上）

    void asyncRecvProtocolContinue(size_t header_len = PROTOCOLHEADER_LEN);
    void asyncRecvBodyContinue(buffer_shared_ptr buf, size_t body_len, size_t header_len = PROTOCOLHEADER_LEN);

    void asyncRecv(buffer_shared_ptr buf, size_t len);          // 异步接收len字节数据到buf
    void asyncRecv(buffer_shared_ptr buf, size_t len, std::function<void()> fun);    // 传递函数对象，异步接收


    void SR_run();      // 在新线程开始异步任务
    void SR_run_local();// 在本地线程开始异步任务
    void SR_stop();     // 结束异步任务
    // 异步信号
signals:
    void connected();               // 连接成功
    void sendOK();                  // 发生成功
    void recvOK();                  // 接收成功
    void error(QString message);    // 错误信号

    // 服务端发送PDU，并且已经接收完成的信号
    void recvPDUOK(std::shared_ptr<PDU> pdu);
    void recvPDURespondOK(std::shared_ptr<PDURespond> pdu);
    void recvTranPduOK(std::shared_ptr<TranPdu> pdu);
    void recvTranDataPduOK(std::shared_ptr<TranDataPdu> pdu);
    void recvTranFinishPduOK(std::shared_ptr<TranFinishPdu> pdu);
    void recvRespondPackOK(std::shared_ptr<RespondPack> pdu);
    void recvUserInfoOK(std::shared_ptr<UserInfo> pdu);
    void recvFileInfoOK(std::shared_ptr<FileInfo> pdu);

private:
    //异步回调函数
    void connectHandler(const boost::system::error_code& ec);   // 异步连接成功后回调函数
    void sendHandler(const boost::system::error_code& ec, std::size_t bytes_transferred);   // 异步发送成功回调函数
    void recvHandler(const boost::system::error_code& ec, std::size_t bytes_transferred);   // 异步接收成功回调函数

private:
    std::shared_ptr<boost::asio::io_context> context_;          // IO 上下文
    std::shared_ptr<boost::asio::ssl::context>	ssl_context_;   // ssl 安全连接上下文，用于配置 SSL/TLS 连接的参数
    sslsock_ptr ssl_sock_;                   // sock安全套接字
    boost::asio::ip::tcp::endpoint ep_;     // 保存对端地址和端口信息，即服务器地址和端口
    std::atomic<bool> running_ = true;
};
