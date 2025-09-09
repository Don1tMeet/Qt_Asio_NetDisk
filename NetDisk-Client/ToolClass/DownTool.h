#ifndef DOWNTOOL_H
#define DOWNTOOL_H

#include <QObject>


#include <QObject>
#include <QFile>
#include <QString>
#include "SR_Tool.h"
#include "protocol.h"

struct DownContext {
    TranPdu pdu{ {0} };         // 文件PDU
    uint64_t recv_bytes{ 0 };   // 已接收字节数
    QByteArray file_hash;       // 文件哈希码（用于验证）
    QFile file;

};

// 处理下载的类
class DownTool : public QObject {
    Q_OBJECT

public:
    DownTool(const QString &ip, const std::uint32_t port, const TranPdu &pdu, const QString &file_path, QObject *parent = nullptr);
    ~DownTool();

signals:
    void sendProgress(qint64 bytes, qint64 total);      // 发送进度信号
    void workFinished();                                // 任务完成信号
    void error(QString message);                        // 错误信号

public slots:
    void doingDown();   // 执行下载操作

    void sendPauseRequest();
    void sendResumeRequest();
    void sendCancelRequest();

private:
    bool sendTranPdu();         // 发送TranPdu
    bool verifySHA256();        // 哈希检查
    bool removeFile();          // 移除文件

private slots:
    void handleRecvPDURespond(std::shared_ptr<PDURespond> pdu);
    void handleRecvTranDataPdu(std::shared_ptr<TranDataPdu> pdu);

private:
    void handleGetsRespond(std::shared_ptr<PDURespond> pdu);
    void handleGetsFinishRespond(std::shared_ptr<PDURespond> pdu);

private:
    void initSignals();

private:
    std::shared_ptr<SR_Tool> sr_tool_;  // 发送接收工具
    DownContext file_ctx_;              // 文件上下文

    boost::system::error_code ec_;      // 错误码
};

#endif // DOWNTOOL_H
