#include "SR_Tool.h"
#include "Serializer.h"
#include "BufferPool.h"
#include <thread>
#include <chrono>

SR_Tool::SR_Tool(const std::string &ep_addr, const int &ep_port, QObject *parent)
    : QObject(parent), ep_(boost::asio::ip::make_address(ep_addr), ep_port)
{
    context_ = std::make_shared<boost::asio::io_context>();
    // tlsv13_client 表示使用 TLSv1.3 协议的客户端模式
    ssl_context_ = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv13_client);
    // 配置 SSL 上下文，使其使用系统默认的证书路径来验证服务器证书。这样可以确保客户端信任操作系统预安装的根证书颁发机构（CA）
    ssl_context_->set_default_verify_paths();
    ssl_sock_ = std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(*context_.get(), *ssl_context_.get());

    run_threads_.clear();
}

SR_Tool::~SR_Tool() {
    SR_stop();

    // 如果ssl_sock_为空或SSL底层TCP已经关闭（TCP关闭，说明SSL也关闭了），直接返回
    if (!ssl_sock_ || !ssl_sock_->lowest_layer().is_open()) {
        return;
    }

    // 如果SSL底层未关闭，则SSL也可能未关闭
    // 关闭SSL
    boost::system::error_code ec;
    ssl_sock_->shutdown(ec);

    // 无论SSL关闭是否成功，强制关闭底层TCP
    ssl_sock_->lowest_layer().close(ec);
    if (ec) {
        qDebug() << "TCP close failed: " << ec.message();
    }
}

// 同步连接，完成后的连接状态保存在 ec
void SR_Tool::connect(boost::system::error_code &ec) {
    // 由于 SSL 是 TCP 之上的协议，因此要先执行 TCP 握手，在执行 SSL 握手
    ssl_sock_->lowest_layer().connect(ep_, ec);     // 底层（TCP）套接字连接
    ssl_sock_->handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client, ec);    // ssl 握手，第一个参数指定是客户端（区别于服务端）
}

// 同步发送，完成后的连接状态保存在 ec，（有问题：不一定能全部写入）
void SR_Tool::send(char *buf, size_t len, boost::system::error_code &ec) {
    // asio的io操作统一了类型，该类型需要满足MutableBufferSequence
    // 但我们不需要自己实现，使用boost::asio::buffer封装要发送的资源
    ssl_sock_->write_some(boost::asio::buffer(buf, len), ec);
}

// 同步接收，完成后的连接状态保存在 ec
void SR_Tool::recv(char *buf, size_t len, boost::system::error_code &ec) {
    // ssl_sock_->read_some()，不一定会读取完整数据，我们需要自己处理
    // 因此使用 boost::asio::read()
    // 它通过内部循环调用传入流（在这里为ssl_sock_）的read_some() 来保证读取到完整数据
    // 同样，使用buffer包装
    boost::asio::read(*ssl_sock_.get(), boost::asio::buffer(buf, len), ec);
}

// 返回ssl_sock_指针
boost::asio::ssl::stream<boost::asio::ip::tcp::socket> *SR_Tool::getSSL() {
    return ssl_sock_.get();
}

// 返回对端对象
boost::asio::ip::tcp::endpoint SR_Tool::getEndpoint() {
    return ep_;
}

// 同步发送PDU到服务端
bool SR_Tool::sendPDU(const PDU &pdu) {
    auto buf = Serializer::serialize(pdu);  // 序列化
    boost::system::error_code ec;
    send(buf.get(), PROTOCOLHEADER_LEN + pdu.header.body_len, ec);

    if (ec) {  // 出错问题
        emit error(QString::fromStdString("send error " + ec.message()));
        return false;
    }
    return true;
}

// 同步接收一个回复（将缓冲区内的内容，组装成RespondPack）
bool SR_Tool::recvRespond(RespondPack &res) {
    // !!!!!!!!!!!!!!!!!!!!! 接收数据后续采用事件循环处理 !!!!!!!!!!!!!!!!!!!!!!!!!!!!
    boost::system::error_code ec;
    recv((char*)&res.code, sizeof(res.code), ec);   // 读取code码
    if (ec) {
        return false;
    }
    recv((char*)&res.len, sizeof(res.len), ec);     // 读取回复体长度
    if (ec) {
        return false;
    }

    if (res.len > 0) {
        recv(res.reserve, res.len, ec);     // 读取回复体内容
    }

    if (ec) {
        emit error(QString::fromStdString("recv error " + ec.message()));
        return false;
    }
    return true;
}

// 接收一条文件信息（将缓冲区内的内容，组装成FileInfo）
bool SR_Tool::recvFileInfo(FileInfo &file_info) {
    boost::system::error_code ec;

    recv((char*)&file_info.file_id, sizeof(file_info.file_id), ec);
    if (ec) return false;

    recv(file_info.file_name, sizeof(file_info.file_name), ec);
    if (ec) return false;

    recv((char*)&file_info.dir_grade, sizeof(file_info.dir_grade), ec);
    if (ec) return false;

    recv(file_info.file_type, sizeof(file_info.file_type), ec);
    if (ec) return false;

    recv((char*)&file_info.file_size, sizeof(file_info.file_size), ec);
    if (ec) return false;

    recv((char*)&file_info.parent_dir, sizeof(file_info.parent_dir), ec);
    if (ec) return false;

    recv(file_info.file_date, sizeof(file_info.file_date), ec);
    if (ec) return false;

    return true; // 没有发生错误
}

// 异步连接，连接完成调用 connectHandler：发射 connected 信号
// void SR_Tool::asyncConnect() {
//     //底层套接字异步连接，成功后，进行异步 ssl 握手
//     auto self = shared_from_this();
//     ssl_sock_->lowest_layer().async_connect(ep_,
//         [self](const boost::system::error_code& ec) {
//             if (!ec) {
//                 self->ssl_sock_->async_handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client,
//                     std::bind(&SR_Tool::connectHandler, self, std::placeholders::_1));
//             }
//             else {
//                 emit self->error(QString::fromLocal8Bit(ec.message()));
//             }
//         });
// }

// 异步连接，连接完成调用传入的函数
// void SR_Tool::asyncConnect(std::function<void ()> fun) {
//     auto self = shared_from_this();

//     ssl_sock_->lowest_layer().async_connect(ep_,
//         [self, fun](const boost::system::error_code& ec) {
//             if (!ec) {
//                 self->ssl_sock_->async_handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client,
//                     [self, fun](const boost::system::error_code& ec) {
//                         if (!ec) {
//                             fun();
//                         }
//                         else {
//                             emit emit self->error(QString::fromLocal8Bit(ec.message()));
//                         }
//                     });
//             }
//             else {
//                 emit self->error(QString::fromLocal8Bit(ec.message()));
//             }
//         });

// }

// 异步发送：将 buf 的 len 字节发送到对端，成功调用 sendHandler：发射 sendOK 信号
// void SR_Tool::asyncSend(buffer_shared_ptr buf, size_t len) {
//     auto self = shared_from_this();
//     qDebug() << "asyncWrite signup success";
//     boost::asio::async_write(*ssl_sock_.get(), boost::asio::buffer(buf.get(), len),
//         [self, buf](const boost::system::error_code& ec, size_t bytes_transferred) {
//             self->sendHandler(ec, bytes_transferred);
//         });
// }

// 异步发送：同 asyncSend(buffer_shared_ptr buf, size_t len)，但成功后调用给定的函数，而不是 sendHandler
// void SR_Tool::asyncSend(buffer_shared_ptr buf, size_t len, std::function<void ()> fun) {
//     auto self = shared_from_this();
//     boost::asio::async_write(*ssl_sock_.get(), boost::asio::buffer(buf.get(), len),
//         [self, fun, buf](const boost::system::error_code& ec, std::size_t bytes_transferred) {
//             if (!ec) {
//                 fun();
//             }
//             else {
//                 emit self->error(QString::fromLocal8Bit(ec.message()));
//             }
//         });
// }

// void SR_Tool::asyncSendFileDataStart(UpContext& file_ctx) {
//     // 预处理
//     if (!file_ctx.file.isOpen()) {
//         emit error("upload file data error: file not open");
//         return;
//     }
//     if (file_ctx.file.size() != static_cast<int64_t>(file_ctx.total_bytes)) {
//         emit error("upload file data error: file size error");
//         return;
//     }

//     // 开始发送数据
//     qDebug() << "SR_Tool: asyncSendFileDataContinue: start";
//     asyncSendFileDataContinue(file_ctx, 0);
// }

// void SR_Tool::asyncSendFileDataContinue(UpContext& file_ctx, uint32_t cur_chunk_id) {
//     if (cur_chunk_id == file_ctx.total_chunks) { // 发送完所有数据
//         return;
//     }
//     // 防止一直占用cpu
//     std::this_thread::sleep_for(std::chrono::milliseconds(1));

//     auto self = shared_from_this();
//     // 设置TranDataPud
//     TranDataPdu pdu;
//     pdu.header.type = ProtocolType::TRANDATAPDU_TYPE;
//     pdu.header.body_len = TRANDATAPDU_BODY_BASE_LEN + (cur_chunk_id == file_ctx.total_chunks-1 ? file_ctx.last_chunk_size : file_ctx.chunk_size);
//     pdu.code = Code::PUTS_DATA;
//     // pdu.status = 0;
//     pdu.file_offset = static_cast<uint64_t>(cur_chunk_id) * file_ctx.chunk_size;
//     pdu.chunk_size = (cur_chunk_id == file_ctx.total_chunks-1 ? file_ctx.last_chunk_size : file_ctx.chunk_size);
//     pdu.total_chunks = file_ctx.total_chunks;
//     pdu.chunk_index = cur_chunk_id;
//     // pdu.check_sum = 0;

//     // 读取文件数据
//     if (!file_ctx.file.isOpen()) {
//         emit error("send file data error: file not open");
//         return;
//     }
//     file_ctx.file.seek(pdu.file_offset);
//     QByteArray byte_chunk = file_ctx.file.read(pdu.chunk_size);
//     if (byte_chunk.size() != pdu.chunk_size) {
//         emit error("send file data error: read file data faild");
//         return;
//     }
//     pdu.data.assign(byte_chunk.constData(), pdu.chunk_size);

//     // 传输控制
//     if (file_ctx.ctrl->load() == 1) {       // 为 1 则暂停等待
//         std::unique_lock<std::mutex>lock(*file_ctx.mtx.get());
//         file_ctx.cv->wait(lock, [ctrl = file_ctx.ctrl] {return ctrl->load() != 1; });
//     }
//     else if (file_ctx.ctrl->load() == 2) {  // 为 2 则结束传输
//         // !!!!!!!!!!!!!!!!!!!!!!!!! 应该发送信息通知服务端取消上传 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//         return;
//     }
//     // 为 0 则继续传输

//     auto buf = Serializer::serialize(pdu);

//     boost::asio::async_write(*ssl_sock_.get(), boost::asio::buffer(buf.get(), PROTOCOLHEADER_LEN + pdu.header.body_len),
//         [self, buf, &file_ctx, cur_chunk_id](const boost::system::error_code& ec, size_t bytes_transferred) {

//             // 持续发送数据
//             self->asyncSendFileDataContinue(file_ctx, cur_chunk_id + 1);
//         });
// }

// 异步接收：将读缓冲区的 len 字节保存到 buf，成功后调用 recvHeaderHandler
// void SR_Tool::asyncRecvProtocol(buffer_shared_ptr buf, size_t header_len) {
//     auto self = shared_from_this();

//     boost::asio::async_read(*ssl_sock_.get(), boost::asio::buffer(buf.get(), header_len),
//         [self, header_len, buf](const boost::system::error_code& ec, std::size_t bytes_transferred) {
//             if (ec) {
//                 emit self->error(QString::fromLocal8Bit(ec.message()));
//                 return;
//             }
//             if (header_len != bytes_transferred) {
//                 emit self->error("recv ProtocolHeader failed: The data received is not as expected");
//                 return;
//             }
//             // 反序列化header
//             ProtocolHeader header;
//             if (!Serializer::deserialize(buf.get(), header_len, header)) {
//                 emit self->error("deserialize ProtocolHeader failed");
//                 return;
//             }
//             if (header.body_len == 0) { // 一般不可能出现该情况
//                 emit self->error("recv Protocol failed: no body");
//                 return;
//             }
//             // 接收body
//             self->asyncRecvBody(buf, header.body_len, header_len);
//         });
// }

// 异步接收：在已接受header的基础上接收body，保存在buf中（header之后）
// void SR_Tool::asyncRecvBody(buffer_shared_ptr buf, size_t body_len, size_t header_len) {
//     auto self = shared_from_this();

//     boost::asio::async_read(*ssl_sock_.get(), boost::asio::buffer(buf.get() + header_len, body_len),
//         [self, header_len, body_len, buf](const boost::system::error_code& ec, std::size_t bytes_transferred) {
//             if (ec) {
//                 emit self->error(QString::fromLocal8Bit(ec.message()));
//                 return;
//             }
//             if (body_len != bytes_transferred) {
//                 emit self->error("recv ProtocolBody failed: The data received is not as expected");
//                 return;
//             }
//             // 反序列化header
//             // !!!!!!!!!!!!!!!!!!!!!!! 这里又解析了一次header，可优化 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//             ProtocolHeader header;
//             if (!Serializer::deserialize(buf.get(), header_len, header)) {
//                 emit self->error("deserialize ProtocolHeader failed");
//                 return;
//             }
//             // 根据header类型，反序列化并发送信号
//             switch (header.type) {
//                 case ProtocolType::PDU_TYPE: {
//                     std::shared_ptr<PDU> pdu = std::make_shared<PDU>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvPDUOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::PDURESPOND_TYPE: {
//                     std::shared_ptr<PDURespond> pdu = std::make_shared<PDURespond>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvPDURespondOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::TRANPDU_TYPE: {
//                     std::shared_ptr<TranPdu> pdu = std::make_shared<TranPdu>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvTranPduOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::TRANDATAPDU_TYPE: {
//                     std::shared_ptr<TranDataPdu> pdu = std::make_shared<TranDataPdu>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvTranDataPduOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::TRANFINISHPDU_TYPE: {
//                     std::shared_ptr<TranFinishPdu> pdu = std::make_shared<TranFinishPdu>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvTranFinishPduOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::RESPONDPACK_TYPE: {
//                     std::shared_ptr<RespondPack> pdu = std::make_shared<RespondPack>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvRespondPackOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::USERINFO_TYPE: {
//                     std::shared_ptr<UserInfo> pdu = std::make_shared<UserInfo>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvUserInfoOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::FILEINFO_TYPE: {
//                     std::shared_ptr<FileInfo> pdu = std::make_shared<FileInfo>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvFileInfoOK(pdu);
//                     }
//                     break;
//                 }
//                 default: {
//                     emit self->error("unknown ProtocolType");
//                     break;
//                 }
//             }
//         });
// }

// void SR_Tool::asyncRecvProtocolContinue(size_t header_len) {
//     auto buf = BufferPool::getInstance().acquire();
//     auto self = shared_from_this();

//     boost::asio::async_read(*ssl_sock_.get(), boost::asio::buffer(buf.get(), header_len),
//         [self, header_len, buf](const boost::system::error_code& ec, std::size_t bytes_transferred) {
//             if (ec) {
//                 emit self->error(QString::fromLocal8Bit(ec.message()));
//                 return;
//             }
//             if (header_len != bytes_transferred) {
//                 emit self->error("recv ProtocolHeader failed: The data received is not as expected");
//                 return;
//             }
//             // 反序列化header
//             ProtocolHeader header;
//             if (!Serializer::deserialize(buf.get(), header_len, header)) {
//                 emit self->error("deserialize ProtocolHeader failed");
//                 return;
//             }
//             if (header.body_len == 0) { // 一般不可能出现该情况
//                 emit self->error("recv Protocol failed: no body");
//                 return;
//             }
//             // 接收body
//             self->asyncRecvBodyContinue(buf, header.body_len, header_len);
//         });
// }

// void SR_Tool::asyncRecvBodyContinue(buffer_shared_ptr buf, size_t body_len, size_t header_len) {
//     auto self = shared_from_this();

//     boost::asio::async_read(*ssl_sock_.get(), boost::asio::buffer(buf.get() + header_len, body_len),
//         [self, header_len, body_len, buf](const boost::system::error_code& ec, std::size_t bytes_transferred) {
//             if (ec) {
//                 emit self->error(QString::fromLocal8Bit(ec.message()));
//                 return;
//             }
//             if (body_len != bytes_transferred) {
//                 emit self->error("recv ProtocolBody failed: The data received is not as expected");
//                 return;
//             }
//             // 反序列化header
//             // !!!!!!!!!!!!!!!!!!!!!!! 这里又解析了一次header，可优化 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//             ProtocolHeader header;
//             if (!Serializer::deserialize(buf.get(), header_len, header)) {
//                 emit self->error("deserialize ProtocolHeader failed");
//                 return;
//             }
//             // 根据header类型，反序列化并发送信号
//             switch (header.type) {
//                 case ProtocolType::PDU_TYPE: {
//                     std::shared_ptr<PDU> pdu = std::make_shared<PDU>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvPDUOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::PDURESPOND_TYPE: {
//                     std::shared_ptr<PDURespond> pdu = std::make_shared<PDURespond>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvPDURespondOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::TRANPDU_TYPE: {
//                     std::shared_ptr<TranPdu> pdu = std::make_shared<TranPdu>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvTranPduOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::TRANDATAPDU_TYPE: {
//                     std::shared_ptr<TranDataPdu> pdu = std::make_shared<TranDataPdu>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvTranDataPduOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::TRANFINISHPDU_TYPE: {
//                     std::shared_ptr<TranFinishPdu> pdu = std::make_shared<TranFinishPdu>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvTranFinishPduOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::RESPONDPACK_TYPE: {
//                     std::shared_ptr<RespondPack> pdu = std::make_shared<RespondPack>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvRespondPackOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::USERINFO_TYPE: {
//                     std::shared_ptr<UserInfo> pdu = std::make_shared<UserInfo>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvUserInfoOK(pdu);
//                     }
//                     break;
//                 }
//                 case ProtocolType::FILEINFO_TYPE: {
//                     std::shared_ptr<FileInfo> pdu = std::make_shared<FileInfo>();
//                     if (Serializer::deserialize(buf.get(), header_len+body_len, *pdu)) {
//                         emit self->recvFileInfoOK(pdu);
//                     }
//                     break;
//                 }
//                 default: {
//                     emit self->error("unknown ProtocolType");
//                     break;
//                 }
//             }
//             self->asyncRecvProtocolContinue();
//         });
// }

// 异步接收：将读缓冲区的 len 字节保存到 buf，成功后调用 recvHandler
// void SR_Tool::asyncRecv(buffer_shared_ptr buf, size_t len) {
//     auto self = shared_from_this();
//     boost::asio::async_read(*ssl_sock_.get(), boost::asio::buffer(buf.get(), len),
//         [self, buf](const boost::system::error_code& ec, std::size_t bytes_transferred) {
//             self->recvHandler(bytes_transferred);
//         });
// }

// 异步接收：同 asyncRecv(buffer_shared_ptr buf, size_t len)，但成功后调用给定的函数，而不是 recvHandler
// void SR_Tool::asyncRecv(buffer_shared_ptr buf, size_t len, std::function<void ()> fun) {
//     auto self = shared_from_this();
//     boost::asio::async_read(*ssl_sock_.get(), boost::asio::buffer(buf.get(), len),
//         [self, fun, buf](const boost::system::error_code& ec, std::size_t bytes_transferred) {
//             if (!ec) {
//                 fun();
//             }
//             else {
//                 emit self->error(QString::fromLocal8Bit(ec.message()));
//             }
//         });
// }

// 异步连接到服务器，连接完成后调用 fun，未传入 fun 调用connectHandler
void SR_Tool::asyncConnect(std::function<void()> fun) {
    auto self = shared_from_this();

    // 启动协程
    boost::asio::co_spawn(
        ssl_sock_->lowest_layer().get_executor(),
        [self, fun]() -> boost::asio::awaitable<void> {
            try {
                // 异步连接
                co_await self->ssl_sock_->lowest_layer().async_connect(
                    self->ep_,
                    boost::asio::use_awaitable
                );

                // 连接成功，进行SSL握手
                co_await self->ssl_sock_->async_handshake(
                    boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client,
                    boost::asio::use_awaitable
                );

                // 握手成功，调用处理函数
                if (fun) {
                    fun();
                }
                else {
                    self->connectHandler();
                }
            }
            catch (const boost::system::system_error& e) {
                emit self->error(QString::fromLocal8Bit(e.what()));
            }
        },
        boost::asio::detached  // 协程独立运行，不阻塞接口调用
    );
}

// 异步发送数据，成功调用 fun，fun 未定义，调用 sendHandler
void SR_Tool::asyncSend(buffer_shared_ptr buf, size_t len, std::function<void()> fun) {
    auto self = shared_from_this();

    // 启动协程
    boost::asio::co_spawn(
        ssl_sock_->get_executor(),  // 使用套接字关联的执行器
        [self, buf, len, fun]() -> boost::asio::awaitable<void> {
            try {
                // 协程等待异步发送完成
                size_t bytes_transferred = co_await boost::asio::async_write(
                    *self->ssl_sock_.get(),
                    boost::asio::buffer(buf.get(), len),
                    boost::asio::use_awaitable  // 使用use_awaitable替代回调
                );

                // 发送成功，调用处理函数
                if (fun) {
                    fun();
                }
                else{
                    self->sendHandler(bytes_transferred);
                }
            }
            catch (const boost::system::system_error& e) {
                emit self->error(QString::fromLocal8Bit(e.what()));
            }
            co_return;
        },
        boost::asio::detached  // 协程独立运行，不阻塞调用者
    );
}

void SR_Tool::asyncRecv(buffer_shared_ptr buf, size_t len, std::function<void ()> fun) {
    auto self = shared_from_this();

    boost::asio::co_spawn(
        ssl_sock_->get_executor(),
        [self, buf, len, fun]() -> boost::asio::awaitable<void> {
            try {
                size_t bytes_transferred = co_await boost::asio::async_read(
                    *self->ssl_sock_.get(),
                    boost::asio::buffer(buf.get(), len),
                    boost::asio::use_awaitable
                );

                // 发送成功，调用处理函数
                if (fun) {
                    fun();
                }
                else{
                    self->recvHandler(bytes_transferred);
                }
            }
            catch (const boost::system::system_error& e) {
                emit self->error(QString::fromLocal8Bit(e.what()));
            }
            co_return;
        },
        boost::asio::detached
    );
}

void SR_Tool::asyncSendFileData(UpContext &file_ctx) {
    if (!file_ctx.file.isOpen()) {
        emit error("upload file data error: file not open");
        return;
    }
    if (file_ctx.file.size() != static_cast<int64_t>(file_ctx.total_bytes)) {
        emit error("upload file data error: file size error");
        return;
    }

    // 开始发送数据
    qDebug() << "SR_Tool: asyncSendFileDataContinue: start";

    auto self = shared_from_this();

    boost::asio::co_spawn(
        ssl_sock_->get_executor(),  // 使用套接字关联的执行器
        [self, &file_ctx]() -> boost::asio::awaitable<void> {
            for (uint32_t cur_chunk_id=0; cur_chunk_id<file_ctx.total_chunks; ++cur_chunk_id) {
                try {
                    // 防止一直占用cpu
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

                    // 设置TranDataPud
                    TranDataPdu pdu;
                    pdu.header.type = ProtocolType::TRANDATAPDU_TYPE;
                    pdu.header.body_len = TRANDATAPDU_BODY_BASE_LEN + (cur_chunk_id == file_ctx.total_chunks-1 ? file_ctx.last_chunk_size : file_ctx.chunk_size);
                    pdu.code = Code::PUTS_DATA;
                    // pdu.status = 0;
                    pdu.file_offset = static_cast<uint64_t>(cur_chunk_id) * file_ctx.chunk_size;
                    pdu.chunk_size = (cur_chunk_id == file_ctx.total_chunks-1 ? file_ctx.last_chunk_size : file_ctx.chunk_size);
                    pdu.total_chunks = file_ctx.total_chunks;
                    pdu.chunk_index = cur_chunk_id;
                    // pdu.check_sum = 0;

                    // 读取文件数据
                    if (!file_ctx.file.isOpen()) {
                        emit self->error("send file data error: file not open");
                        co_return;
                    }
                    file_ctx.file.seek(pdu.file_offset);
                    QByteArray byte_chunk = file_ctx.file.read(pdu.chunk_size);
                    if (byte_chunk.size() != pdu.chunk_size) {
                        emit self->error("send file data error: read file data faild");
                        co_return;
                    }
                    pdu.data.assign(byte_chunk.constData(), pdu.chunk_size);

                    // 传输控制
                    if (file_ctx.ctrl->load() == 1) {       // 为 1 则暂停等待
                        std::unique_lock<std::mutex>lock(*file_ctx.mtx.get());
                        file_ctx.cv->wait(lock, [ctrl = file_ctx.ctrl] { return ctrl->load() != 1; });
                    }
                    else if (file_ctx.ctrl->load() == 2) {  // 为 2 则结束传输
                        // !!!!!!!!!!!!!!!!!!!!!!!!! 应该发送信息通知服务端取消上传 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        co_return;
                    }
                    // 为 0 则继续传输

                    // 发送数据
                    auto buf = Serializer::serialize(pdu);
                    size_t bytes_transferred = co_await boost::asio::async_write(
                        *self->ssl_sock_.get(),
                        boost::asio::buffer(buf.get(), PROTOCOLHEADER_LEN + pdu.header.body_len),
                        boost::asio::use_awaitable
                    );

                    (void) bytes_transferred;   // 防止未使用 bytes_transferred 警告
                }
                catch (const boost::system::system_error& e) {
                    emit self->error(QString::fromLocal8Bit(e.what()));
                    co_return;
                }
            }
            co_return;
        },
        boost::asio::detached
    );
}

// 接受服务端发送过来的通信协议，keep 表示是否持续接受
void SR_Tool::asyncRecvProtocol(bool keep, size_t header_len) {
    auto self = shared_from_this();

    boost::asio::co_spawn(
        ssl_sock_->get_executor(),
        [self, keep, header_len]() -> boost::asio::awaitable<void> {
            do {
                auto buf = BufferPool::getInstance().acquire();
                try {
                    // 读取协议头（异步）
                    size_t bytes_transferred = co_await boost::asio::async_read(
                        *self->ssl_sock_.get(),
                        boost::asio::buffer(buf.get(), header_len),
                        boost::asio::use_awaitable
                    );

                    if (header_len != bytes_transferred) {
                        emit self->error("recv ProtocolHeader failed: The data received is not as expected");
                        co_return;
                    }

                    // 反序列化header
                    ProtocolHeader header;
                    if (!Serializer::deserialize(buf.get(), header_len, header)) {
                        emit self->error("deserialize ProtocolHeader failed");
                        co_return;
                    }
                    if (header.body_len == 0) { // 一般不可能出现该情况
                        emit self->error("recv Protocol failed: no body");
                        co_return;
                    }

                    // 读取协议体（异步）
                    bytes_transferred = co_await boost::asio::async_read(
                        *self->ssl_sock_.get(),
                        boost::asio::buffer(buf.get() + header_len, header.body_len),
                        boost::asio::use_awaitable
                    );

                    if (header.body_len != bytes_transferred) {
                        emit self->error("recv ProtocolBody failed: The data received is not as expected");
                        co_return;
                    }

                    // 根据header类型，反序列化并发送信号
                    switch (header.type) {
                        case ProtocolType::PDU_TYPE: {
                            std::shared_ptr<PDU> pdu = std::make_shared<PDU>();
                            if (Serializer::deserialize(buf.get(), header_len+header.body_len, *pdu)) {
                                emit self->recvPDUOK(pdu);
                            }
                            break;
                        }
                        case ProtocolType::PDURESPOND_TYPE: {
                            std::shared_ptr<PDURespond> pdu = std::make_shared<PDURespond>();
                            if (Serializer::deserialize(buf.get(), header_len+header.body_len, *pdu)) {
                                emit self->recvPDURespondOK(pdu);
                            }
                            break;
                        }
                        case ProtocolType::TRANPDU_TYPE: {
                            std::shared_ptr<TranPdu> pdu = std::make_shared<TranPdu>();
                            if (Serializer::deserialize(buf.get(), header_len+header.body_len, *pdu)) {
                                emit self->recvTranPduOK(pdu);
                            }
                            break;
                        }
                        case ProtocolType::TRANDATAPDU_TYPE: {
                            std::shared_ptr<TranDataPdu> pdu = std::make_shared<TranDataPdu>();
                            if (Serializer::deserialize(buf.get(), header_len+header.body_len, *pdu)) {
                                emit self->recvTranDataPduOK(pdu);
                            }
                            break;
                        }
                        case ProtocolType::TRANFINISHPDU_TYPE: {
                            std::shared_ptr<TranFinishPdu> pdu = std::make_shared<TranFinishPdu>();
                            if (Serializer::deserialize(buf.get(), header_len+header.body_len, *pdu)) {
                                emit self->recvTranFinishPduOK(pdu);
                            }
                            break;
                        }
                        case ProtocolType::RESPONDPACK_TYPE: {
                            std::shared_ptr<RespondPack> pdu = std::make_shared<RespondPack>();
                            if (Serializer::deserialize(buf.get(), header_len+header.body_len, *pdu)) {
                                emit self->recvRespondPackOK(pdu);
                            }
                            break;
                        }
                        case ProtocolType::USERINFO_TYPE: {
                            std::shared_ptr<UserInfo> pdu = std::make_shared<UserInfo>();
                            if (Serializer::deserialize(buf.get(), header_len+header.body_len, *pdu)) {
                                emit self->recvUserInfoOK(pdu);
                            }
                            break;
                        }
                        case ProtocolType::FILEINFO_TYPE: {
                            std::shared_ptr<FileInfo> pdu = std::make_shared<FileInfo>();
                            if (Serializer::deserialize(buf.get(), header_len+header.body_len, *pdu)) {
                                emit self->recvFileInfoOK(pdu);
                            }
                            break;
                        }
                        default: {
                            emit self->error("unknown ProtocolType");
                            break;
                        }
                    }
                }
                catch (const boost::system::system_error& e) {
                    emit self->error(QString::fromLocal8Bit(e.what()));
                    co_return;
                }

            } while (keep && self->running_);

            co_return;
        },
        boost::asio::detached
    );
}

// 开始异步任务
void SR_Tool::SR_run() {
    running_ = true;
    auto self = shared_from_this();
    run_threads_.emplace_back(std::thread(
        [self]() {
            try {
                while (self->running_) {
                    if (0 == self->context_->run()) {
                        // 没有异步任务休眠，避免长时间占用cpu
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            }
            catch (const std::exception& e) {
                emit self->error(QString::fromLocal8Bit(e.what()));
                return;
            }
        }
    ));
}

void SR_Tool::SR_run_local() {
    running_ = true;
    try {
        while (running_) {
            if (0 == context_->run()) {
                // 没有异步任务休眠，避免长时间占用cpu
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    catch (const std::exception& e) {
        emit error(QString::fromLocal8Bit(e.what()));
    }
}

// 结束异步任务
void SR_Tool::SR_stop() {
    running_ = false;
    for (auto& th : run_threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
}

// 异步连接成功时的回调函数
void SR_Tool::connectHandler() {
    emit connected();
}

// 发送成功回调函数
void SR_Tool::sendHandler(std::size_t bytes_transferred) {
    (void) bytes_transferred;   // 防止未使用 bytes_transferred 的警告
    emit sendOK();
}

// 接收成功回调函数
void SR_Tool::recvHandler(std::size_t bytes_transferred) {
    (void) bytes_transferred;   // 防止未使用 bytes_transferred 的警告
    emit recvOK();
}
