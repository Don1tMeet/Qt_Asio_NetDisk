#ifndef UDTOOL_H
#define UDTOOL_H

#include <QObject>
#include <QFile>
#include <QString>
#include <mutex>
#include <condition_variable>
// include "SR_Tool.h"
#include "protocol.h"

class SR_Tool;

struct UpContext {
    TranPdu tran_pdu{ {0} };            // 上传下载通信结构体
    QString file_name;                  // 文件名（带路径）
    QFile file;                         // 上传下载文件对象

    uint64_t total_bytes{ 0 };          // 需要发送的总字节数
    uint64_t sended_bytes{ 0 };         // 已发送字节数

    uint32_t total_chunks{ 0 };         // 总chunk数
    const uint32_t chunk_size{ 2048 };  // 每次发送块的大小
    uint32_t last_chunk_size{ 0 };      // 最后一个chunk的大小
    std::set<uint32_t> unacked_id;      // 未确认的chunk id

    // 工作控制，0为继续，1为暂停，2为结束
    std::shared_ptr<std::atomic<std::uint32_t>> ctrl{ nullptr };
    std::shared_ptr<std::condition_variable> cv{ nullptr };
    std::shared_ptr<std::mutex> mtx{ nullptr };
};

// 处理上传（长任务）的类
class UdTool : public QObject {
    Q_OBJECT

public:
    UdTool(const QString& ip, const std::uint32_t& port, QObject *parent = nullptr);
    ~UdTool();

private:
    void initSignals();

public:
    // 设置要上传和下载的文件
    void setTranPdu(const TranPdu& tran_pdu);
    // 设置传输控制变量
    void setControlAtomic(std::shared_ptr<std::atomic<std::uint32_t>> control,
                          std::shared_ptr<std::condition_variable> cv, std::shared_ptr<std::mutex> mutex);

signals:
    void sendProgress(qint64 bytes, qint64 total);      // 发送进度信号
    void workFinished();                                // 任务完成信号
    void sendItemData(TranPdu data, uint64_t file_id);  // 发送添加文件视图信号
    void error(QString message);                        // 错误信号

public slots:
    void doingUp();     // 执行上传操作

    void sendCancelRequest();

private:
    bool calculateSHA256();     // 计算文件哈希值
    bool sendTranPdu();         // 发送TranPdu
    bool sendFile();            // 发送文件

private slots:
    void handleRecvPDURespond(std::shared_ptr<PDURespond> pdu);

private:
    void handlePutsRespond(std::shared_ptr<PDURespond> pdu);
    void handlePutsDataRespond(std::shared_ptr<PDURespond> pdu);
    void handlePutsFinishRespond(std::shared_ptr<PDURespond> pdu);

private:
    std::shared_ptr<SR_Tool> sr_tool_;  // 发送接收工具
    UpContext file_ctx_;

    double last_progress_{ 0 };  // 最后一次进度
    const double progress_step_{ 0.003 };   // 更新进度条的最小进度
};

#endif // UDTOOL_H
