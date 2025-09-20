#include "UdTool.h"
#include "SR_Tool.h"
#include <openssl/sha.h>
#include <QFileInfo>
#include <QThread>

UdTool::UdTool(const QString& ip, const std::uint32_t& port, QObject *parent)
    : QObject{parent}, sr_tool_(std::make_shared<SR_Tool>(ip.toStdString(), port, nullptr))
{
    initSignals();
}

UdTool::~UdTool() {
    sr_tool_->SR_stop();    // 关闭异步任务

    // 关闭文件
    if (file_ctx_.file.isOpen()) {
        file_ctx_.file.close();
    }

    // 关闭连接
    try {
        sr_tool_->getSSL()->shutdown(); // 关闭 SSL
    }
    catch (const boost::system::system_error& e) {
        if (e.code() == boost::asio::ssl::error::stream_truncated) {
            qWarning() << "SSL 关闭警告：流被截断（对方可能已关闭连接）";
        }
        else {
            qWarning() << "SSL 关闭错误：" << e.what();
        }
    }
    // 无论SSL是否关闭，都关闭底层socket
    sr_tool_->getSSL()->lowest_layer().close();
}

void UdTool::initSignals() {
    connect(sr_tool_.get(), &SR_Tool::recvPDURespondOK,
            this, &UdTool::handleRecvPDURespond);
}

// 设置上传的文件
void UdTool::setTranPdu(const TranPdu &tran_pdu) {
    // tran_pdu.file_name实际上保存的是path/name
    if (Code::PUTS != tran_pdu.tran_pdu_code) {
        emit error("UdTool::SetTranPdu(const TranPdu&): tran_pdu_code error");
        return;
    }

    file_ctx_.tran_pdu = tran_pdu;

    file_ctx_.file_name = QString(file_ctx_.tran_pdu.file_name);

    file_ctx_.file.setFileName(file_ctx_.file_name);

    // 打开文件
    if (!file_ctx_.file.open(QIODevice::ReadOnly)) {
        emit error("UdTool::SetTranPdu(const TranPdu&): open file error");
        return;
    }

    file_ctx_.tran_pdu.file_size = file_ctx_.file.size();   // 初始化文件大小

    file_ctx_.total_bytes = file_ctx_.tran_pdu.file_size;   // 文件总大小

    file_ctx_.total_chunks = (file_ctx_.tran_pdu.file_size - 1) / file_ctx_.chunk_size + 1; // 总 chunk 数
    file_ctx_.last_chunk_size = file_ctx_.total_bytes - static_cast<uint64_t>(file_ctx_.total_chunks-1) * file_ctx_.chunk_size; // 最后一个chunk的大小
    // 初始化未确认（未发送成功）的chunks
    for (uint32_t i=0; i<file_ctx_.total_chunks; ++i) {
        file_ctx_.unacked_id.insert(i);
    }

    // 保存文件名（不带路径）
    QFileInfo file_info(file_ctx_.file_name);
    QByteArray file_suffix = file_info.fileName().toUtf8();
    memset(file_ctx_.tran_pdu.file_name, 0, sizeof(file_ctx_.tran_pdu.file_name));
    memcpy(file_ctx_.tran_pdu.file_name, file_suffix.data(), file_suffix.size());
    file_ctx_.tran_pdu.file_name[file_suffix.size()] = '\0';
}

// 设置传输控制对象
void UdTool::setControlAtomic(std::shared_ptr<std::atomic<std::uint32_t>> control,
                              std::shared_ptr<std::condition_variable> cv, std::shared_ptr<std::mutex> mutex)
{
    file_ctx_.ctrl = control;
    file_ctx_.cv = cv;
    file_ctx_.mtx = mutex;
}

// 计算文件哈希值
bool UdTool::calculateSHA256() {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();  // 创建EVP上下文
    if (!ctx) {
        return false;
    }

    const EVP_MD* md = EVP_sha256();  // 获取SHA-256算法对象
    if (!md) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // 初始化上下文
    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    const int buffer_size = 4096;
    char buffer[buffer_size];
    int read_bytes = 0;

    // 读取文件并更新哈希计算
    if (file_ctx_.file.isOpen()) {
        file_ctx_.file.seek(0);
        while ((read_bytes = file_ctx_.file.read(buffer, buffer_size)) > 0) {
            if (EVP_DigestUpdate(ctx, buffer, read_bytes) != 1) {
                EVP_MD_CTX_free(ctx);
                return false;
            }
        }
    }
    else {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // 获取哈希计算结果
    unsigned char result[SHA256_DIGEST_LENGTH];
    unsigned int result_len = 0;
    if (EVP_DigestFinal_ex(ctx, result, &result_len) != 1 || result_len != SHA256_DIGEST_LENGTH) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    EVP_MD_CTX_free(ctx);  // 释放上下文

    // 将 SHA-256 结果转换为 QByteArray
    QByteArray sha256_result(reinterpret_cast<char*>(result), SHA256_DIGEST_LENGTH);
    QByteArray arry_hex = sha256_result.toHex();

    if (sizeof(file_ctx_.tran_pdu.file_md5) < static_cast<unsigned long long>(arry_hex.size()) + 1) {
        return false;
    }

    memcpy(file_ctx_.tran_pdu.file_md5, arry_hex.data(), arry_hex.size());
    file_ctx_.tran_pdu.file_md5[arry_hex.size()] = '\0';
    return true;
}

// 发送TranPdu到服务端
bool UdTool::sendTranPdu() {
    // 序列化
    auto buf = Serializer::serialize(file_ctx_.tran_pdu);

    boost::system::error_code ec;
    sr_tool_->send(buf.get(), PROTOCOLHEADER_LEN + file_ctx_.tran_pdu.header.body_len, ec);

    if (ec) {
        emit error("UdTool::sendTranPdu(): " + QString::fromStdString(ec.what()));
        return false;
    }
    return true;
}

// 开始发送文件
bool UdTool::sendFile() {
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // 这里假设所有数据都能发送成功，后续添加重传机制
    // 这里sr_tool_使用了，file_，因此在销毁UdTool前，因该确保所有异步任务完成
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    qDebug() << "upload file: start send file data";
    sr_tool_->asyncSendFileData(file_ctx_);

    sr_tool_->SR_run(); // 开启一个异步任务循环
    return true;
}

void UdTool::handleRecvPDURespond(std::shared_ptr<PDURespond> pdu) {
    switch (pdu->code) {
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!! 可以将任务交给线程池处理，不然可能处理速度跟不上接收数据 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        case Code::PUTS: handlePutsRespond(pdu); break;
        case Code::PUTS_DATA: handlePutsDataRespond(pdu); break;
        case Code::PUTS_FINISH: handlePutsFinishRespond(pdu); break;
    }
}

void UdTool::handlePutsRespond(std::shared_ptr<PDURespond> pdu) {
    switch (pdu->status) {
        case Status::SUCCESS: {
            sendFile(); // 发送文件数据
            break;
        }
        case Status::PUT_CONTINUE_FAILED: {
            // !!!!!!!!!!!!!!!!!!!! 处理端点续传失败 !!!!!!!!!!!!!!!!!!!!!!!!!
            break;
        }
        case Status::PUT_QUICK: {
            file_ctx_.sended_bytes = file_ctx_.total_bytes;
            file_ctx_.unacked_id.clear();
            emit sendProgress(1, 1);
            break;
        }
        case Status::FAILED: {
            emit error("upload file request error:");
            break;
        }
        case Status::NO_CAPACITY: {
            emit error("upload file request error: no capacity");
            break;
        }
        default: {
            emit error("upload file request error: unknown operator");
            break;
        }
    }
}

void UdTool::handlePutsDataRespond(std::shared_ptr<PDURespond> pdu) {
    if (Status::SUCCESS == pdu->status) {
        uint32_t chunk_id = 0;
        memcpy((char*)&chunk_id, pdu->msg.data(), sizeof(chunk_id));
        chunk_id = ntohl(chunk_id);
        //  !!!!!!!!!!!!!!!!!!!!!!!!!! 这里不需要加锁，因此现在是单线程，但后续如果使用了线程池处理就需要加锁 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        file_ctx_.unacked_id.erase(chunk_id);   // 删除chunk_id
        file_ctx_.sended_bytes += (chunk_id == file_ctx_.total_chunks-1) ? file_ctx_.last_chunk_size : file_ctx_.chunk_size;

        // 更新进度条
        // 如果达到更新条件在更新，避免一直更新卡顿UI，降低效率
        double cur_progress = static_cast<double>(file_ctx_.sended_bytes) / file_ctx_.total_bytes;
        if (cur_progress - last_progress_ >= progress_step_) {
            emit sendProgress(file_ctx_.sended_bytes, file_ctx_.total_bytes);
        }
    }

}

void UdTool::handlePutsFinishRespond(std::shared_ptr<PDURespond> pdu) {
    if (Status::SUCCESS == pdu->status) {
        // 获取file_id
        uint64_t file_id = 0;
        memcpy((char*)&file_id, pdu->msg.data(), sizeof(file_id));
        file_id = ntohll(file_id);

        emit sendItemData(file_ctx_.tran_pdu, file_id); // 在文件视图系统中添加文件项
        // 发送完成确认回复
        TranFinishPdu ack_pdu;
        ack_pdu.header.type = ProtocolType::TRANFINISHPDU_TYPE;
        ack_pdu.header.body_len = TRANFINISHPDU_BODY_LEN;
        ack_pdu.code = Code::PUTS_FINISH;
        ack_pdu.file_size = file_ctx_.total_bytes;
        memcpy(ack_pdu.file_md5, file_ctx_.tran_pdu.file_md5, sizeof(ack_pdu.file_md5));

        auto buf = Serializer::serialize(ack_pdu);

        boost::system::error_code ec;
        sr_tool_->send(buf.get(), PROTOCOLHEADER_LEN + ack_pdu.header.body_len, ec);
        if (ec) {
            // !!!!!!!!!!!!!!!!!!!!! 处理发送错误的情况 !!!!!!!!!!!!!!!!!!!!!!!!
            emit error("upload finish send error:" + QString::fromStdString(ec.what()));
        }
        else {
            emit sendProgress(1, 1);    // 更新进度条
            emit workFinished();        // 完成任务
        }
    }
    else if (Status::FAILED == pdu->status){
        emit error("upload file finish error");
    }
}

// 开始执行任务槽函数，连接QThread的started信号
void UdTool::doingUp() {
    // 连接服务器
    boost::system::error_code ec;
    sr_tool_->connect(ec);
    if (ec) {
        emit error("UdTool::doingUP(): connect error" + QString::fromLocal8Bit(ec.message()));
        return;
    }
    // 计算上传文件的哈希值
    if (!calculateSHA256()) {
        emit error("UdTool::doingUP(): create hash error");
        return;
    }
    // 发送TranPDU
    if(!sendTranPdu()) {
        emit error("UdTool::doingUP(): send pdu error" + QString::fromLocal8Bit(ec.message()));
        return;
    }
    qDebug() << "upload file: start upload";
    // 发送请求成功后，接收回复
    sr_tool_->asyncRecvProtocol(true);  // 持续注册接收回复的异步事件
    sr_tool_->SR_run(); // 在其它线程启动异步事件
}

void UdTool::sendCancelRequest() {
    emit workFinished();    // 发送完成信号，关闭连接和线程
}
