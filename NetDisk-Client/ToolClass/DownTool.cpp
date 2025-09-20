#include "DownTool.h"

DownTool::DownTool(const QString &ip, const uint32_t port, const TranPdu &pdu, const QString &file_path, QObject *parent)
    : QObject{parent}, sr_tool_(std::make_shared<SR_Tool>(ip.toStdString(), port, nullptr))
{
    initSignals();  // 初始化信号

    if (pdu.tran_pdu_code != Code::GETS) {
        qWarning() << "download file error: pdu code not GETS";
        return;
    }

    file_ctx_.pdu = pdu;
    file_ctx_.file.setFileName(file_path);
}

DownTool::~DownTool() {
    sr_tool_->SR_stop();    // 关闭异步任务

    // 关闭文件
    if (file_ctx_.file.exists()) {
        if (file_ctx_.file.isOpen()) {
            file_ctx_.file.close();
        }
    }
    // 再次确保未完成文件被删除
    if (file_ctx_.recv_bytes != file_ctx_.pdu.file_size) {
        removeFile();
    }

    try {
        // 尝试关闭 SSL 流
        sr_tool_->getSSL()->shutdown();
    }
    catch (const boost::system::system_error& e) {
        if (e.code() == boost::asio::ssl::error::stream_truncated) {
            qWarning() << "SSL 关闭警告：流被截断（对方可能已关闭连接）";
        } else {
            qWarning() << "SSL 关闭错误：" << e.what();
        }
    }
    // 无论SSL是否关闭，都关闭底层socket
    sr_tool_->getSSL()->lowest_layer().close();
}

void DownTool::doingDown() {
    // 连接到服务器
    sr_tool_->connect(ec_);
    if (ec_) {
        emit error("DownTool::doingDown(): Connect error: " + QString::fromLocal8Bit(ec_.message()));
        return;
    }

    // 发送下载请求
    if (!sendTranPdu()) {
        emit error("DownTool::doingDown(): Send PDU error: " + QString::fromLocal8Bit(ec_.message()));
        return;
    }

    // 发送请求成功后，接收回复
    sr_tool_->asyncRecvProtocol(true);  // 持续注册接收回复的异步事件
    sr_tool_->SR_run(); // 在其它线程启动异步事件
}

void DownTool::sendPauseRequest() {
    TranControlPdu pdu;
    pdu.header.type = ProtocolType::TRANCONTROLPDU_TYPE;
    pdu.header.body_len = TRANCONTROL_BODY_BASE_LEN;
    pdu.code = Code::GETS_CONTROL;
    pdu.action = ControlAction::PAUSE;

    auto buf = Serializer::serialize(pdu);

    sr_tool_->send(buf.get(), PROTOCOLHEADER_LEN + pdu.header.body_len, ec_);
    if (ec_) {
        emit error("download file error: send control pause failed");
    }
    qDebug() << "download file: send pause request";

}

void DownTool::sendResumeRequest() {
    TranControlPdu pdu;
    pdu.header.type = ProtocolType::TRANCONTROLPDU_TYPE;
    pdu.header.body_len = TRANCONTROL_BODY_BASE_LEN;
    pdu.code = Code::GETS_CONTROL;
    pdu.action = ControlAction::RESUME;

    auto buf = Serializer::serialize(pdu);

    sr_tool_->send(buf.get(), PROTOCOLHEADER_LEN + pdu.header.body_len, ec_);
    if (ec_) {
        emit error("download file error: send control resume failed");
    }
    qDebug() << "download file: send resume request";
}

void DownTool::sendCancelRequest() {
    TranControlPdu pdu;
    pdu.header.type = ProtocolType::TRANCONTROLPDU_TYPE;
    pdu.header.body_len = TRANCONTROL_BODY_BASE_LEN;
    pdu.code = Code::GETS_CONTROL;
    pdu.action = ControlAction::CANCEL;

    auto buf = Serializer::serialize(pdu);

    // 如果文件未下载完，删除文件
    if (file_ctx_.recv_bytes != file_ctx_.pdu.file_size) {
        removeFile();
    }

    sr_tool_->send(buf.get(), PROTOCOLHEADER_LEN + pdu.header.body_len, ec_);
    if (ec_) {
        emit error("download file error: send cancel failed");
    }
    emit workFinished();    // 取消后，发送完成信号，关闭连接和线程
    qDebug() << "download file: send cancel request";
}

// 发送TranPdu到服务端
bool DownTool::sendTranPdu() {
    // 序列化
    auto buf = Serializer::serialize(file_ctx_.pdu);

    sr_tool_->send(buf.get(), PROTOCOLHEADER_LEN + file_ctx_.pdu.header.body_len, ec_);

    if (ec_) {
        emit error("DwonTool::sendTranPdu(): " + QString::fromStdString(ec_.what()));
        return false;
    }
    return true;
}

bool DownTool::verifySHA256() {
    QFile &file = file_ctx_.file;

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
    if (file.isOpen()) {
        file.seek(0);	//重新将文件指针头指针
        while ((read_bytes = file.read(buffer, buffer_size)) > 0) {
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

    file.close();  // 验证完成，说明任务结束，关闭文件

    // 将计算结果转换为 QByteArray
    QByteArray calculated_hash(reinterpret_cast<char*>(result), SHA256_DIGEST_LENGTH);

    // 比较计算的哈希码与服务端发来的哈希码
    if (calculated_hash.toHex() == file_ctx_.file_hash) {
        return true;
    }
    // 验证失败
    qDebug() << "hash verify failed:\n" << "client hash:" << calculated_hash.toHex() << "\n" << "server hash:" << file_ctx_.file_hash;
    return false;
}

// 移除文件
bool DownTool::removeFile() {
    if (file_ctx_.file.exists()) {  // 如果文件存在
        file_ctx_.file.close();
        if (file_ctx_.file.remove()) {
            return true;
        }
        else {
            emit error("UdTool::removeFileIfExists(): Failed to remove file: " + file_ctx_.file.fileName());
            return false;
        }
    }
    return false;
}

void DownTool::handleRecvPDURespond(std::shared_ptr<PDURespond> pdu) {
    switch (pdu->code) {
        case Code::GETS: handleGetsRespond(pdu); break;
        case Code::GETS_FINISH: handleGetsFinishRespond(pdu); break;
    }
}

void DownTool::handleRecvTranDataPdu(std::shared_ptr<TranDataPdu> pdu) {
    if (GETS_DATA ==  pdu->code) {
        QFile &file = file_ctx_.file;
        if (!file_ctx_.file.isOpen()) {
            return;
        }
        // 写入数据不需要加锁保护，因为DownTool是在单线程工作的
        if (file_ctx_.recv_bytes != pdu->file_offset) { // 检查是否漏了数据
            removeFile();
            emit error("download file: recv data error: missing data");
            return;
        }
        file.seek(pdu->file_offset);                    // 定位
        file.write(pdu->data.data(), pdu->chunk_size);  // 写入数据
        file_ctx_.recv_bytes += pdu->chunk_size;        // 更新已接收字节数

        // 更新进度条
        emit sendProgress(file_ctx_.recv_bytes, file_ctx_.pdu.file_size);
        // !!!!!!!!!!!!!!!!!!!!!! 可以增加回复确认的功能，用于服务端重传 !!!!!!!!!!!!!!!!!!!!!!!!!!!!

        qDebug() << "recv bytes:" << file_ctx_.recv_bytes << "total bytes:" << file_ctx_.pdu.file_size;
        // 下载完成
        if (file_ctx_.recv_bytes == file_ctx_.pdu.file_size) {
            // 验证哈希,发送TranFinishPdu回复
            TranFinishPdu pdu;
            pdu.header.type = ProtocolType::TRANFINISHPDU_TYPE;
            pdu.header.body_len = TRANFINISHPDU_BODY_LEN;
            pdu.code = Code::GETS_FINISH;

            if (verifySHA256()) {   // 验证成功
                qDebug() << "download file verify success";
                pdu.file_size = 1;  // 1 表示验证成功
            }
            else {  // 验证失败
                qDebug() << "download file verify failed";
                pdu.file_size = 0;  // 0 表示验证失败
                removeFile();   // 删除文件
            }

            auto buf = Serializer::serialize(pdu);
            sr_tool_->send(buf.get(), PROTOCOLHEADER_LEN + pdu.header.body_len, ec_);
            if (ec_) {
                removeFile();
                emit error("download file error: send finish pdu failed");
                return;
            }
        }
    }
    else {
        removeFile();
        emit error("recv file data error: error pdu code");
    }
}

void DownTool::handleGetsRespond(std::shared_ptr<PDURespond> pdu) {
    switch (pdu->status) {
        case Status::SUCCESS: {
            // 获取文件大小
            uint64_t file_size = 0;
            memcpy((char*)&file_size, pdu->msg.data(), sizeof(file_size));
            file_ctx_.pdu.file_size = ntohll(file_size);
            // 保存文件哈希码，用于验证
            file_ctx_.file_hash = QByteArray(pdu->msg.data() + sizeof(file_size));

            // 打开文件
            if (!file_ctx_.file.open(QIODevice::ReadWrite)) {
                emit error("DownTool::recvFile(): Error opening file for writing");
                break;
            }

            qDebug() << "recv download file start request ok";

            // 发送一个TranDataPdu给服务器，告诉服务器开始发送文件数据
            TranDataPdu request_pdu;
            request_pdu.header.type = ProtocolType::TRANDATAPDU_TYPE;
            request_pdu.header.body_len = TRANDATAPDU_BODY_BASE_LEN;
            request_pdu.code = Code::GETS_DATA;

            auto buf = Serializer::serialize(request_pdu);
            sr_tool_->send(buf.get(), PROTOCOLHEADER_LEN + request_pdu.header.body_len, ec_);
            if (ec_) {
                removeFile();
                emit error("download file error: request data failed: " + QString::fromLocal8Bit(ec_.message()));
                return;
            }

            qDebug() << "send recv file data request ok";
            break;
        }
        case Status::FILE_NOT_EXIST: {
            emit error("download file error: file not exist");
            break;
        }
        case Status::GET_CONTINUE_FAILED: {
            emit error("download file error: get continue failed");
            break;
        }
        case Status::FAILED: {
            emit error("download file error: request error");
            break;
        }
        default: {
            break;
        }
    }
}

void DownTool::handleGetsFinishRespond(std::shared_ptr<PDURespond> pdu) {
    switch (pdu->status) {
        case Status::SUCCESS: {
            emit workFinished();
            break;
        }
        case Status::FAILED: {
            removeFile();
            emit error("download file error: verify failed");
            break;
        }
    }
}

void DownTool::initSignals() {
    connect(sr_tool_.get(), &SR_Tool::recvPDURespondOK,
            this, &DownTool::handleRecvPDURespond);
    connect(sr_tool_.get(), &SR_Tool::recvTranDataPduOK,
            this, &DownTool::handleRecvTranDataPdu);
}
