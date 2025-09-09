#ifndef SHORTTASKMANAGER_H
#define SHORTTASKMANAGER_H

#include <QObject>
#include <vector>
#include "SR_Tool.h"
#include "TaskQue.h"
#include "protocol.h"


// 短任务管理器
class ShortTaskManager : public QObject {
    Q_OBJECT

public:
    ShortTaskManager(std::shared_ptr<SR_Tool> sr_tool, std::shared_ptr<TaskQue> task_queue, QObject* parent = nullptr);
    ~ShortTaskManager();

public: // 操作请求（只发出请求，不处理接收数据与处理数据）
    void getFileList();         // 获取文件列表
    void makeDir(quint64 parent_id, QString new_dir_name);  // 创建文件夹
    void delFile(quint64 file_id, QString file_name);       // 删除文件

signals:
    void getFileListOK(std::shared_ptr<std::vector<FileInfo>> vet);
    void createDirOK(quint64 new_id,quint64 parent_id,QString new_dir_name);
    void delFileOK();
    void error(QString message);

public slots:   // 处理SR_Tool信号的槽函数
    void handleRecvPDURespond(std::shared_ptr<PDURespond> pdu); // 暂不使用
    void handleRecvFileInfo(std::shared_ptr<FileInfo> pdu);

private:    // 处理接收到信息的函数
    void handleCD(std::shared_ptr<PDURespond> pdu);
    void handleMakeDir(std::shared_ptr<PDURespond> pdu);
    void handleDeleteFile(std::shared_ptr<PDURespond> pdu);
    void handleFileInfo(std::shared_ptr<FileInfo> pdu);

private:
    void initSignals();

private:
    // 注意声明顺序，file_mutex_，file_cnt_，file_vet_可能再task_queue_中被使用
    // 因此需要先销毁task_queue_，也就是最后声明task_queue_
    std::mutex file_mutex_;                 // 保护文件操作的锁
    std::atomic<uint32_t> file_cnt_{ 0 };   // 需要接收的文件数量，用于CD操作
    std::shared_ptr<std::vector<FileInfo>> file_vet_;   // 保存获取的文件数据，用于CD操作

    std::shared_ptr<SR_Tool> sr_tool_;
    std::shared_ptr<TaskQue> task_queue_;   // 任务队列
};

#endif // SHORTTASKMANAGER_H
