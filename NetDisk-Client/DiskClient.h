#ifndef DISKCLIENT_H
#define DISKCLIENT_H

#include <QWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "SR_Tool.h"
#include "TaskQue.h"
#include "ShortTaskManager.h"
// #include "FileViewSystem.h"
#include "FileSystem.h"
#include "protocol.h"

class DiskClient : public QWidget {
    Q_OBJECT

public:
    DiskClient(QWidget *parent = nullptr);
    ~DiskClient();
    bool startClient();

private:
    void initUI();
    void applyQSS();
    void initSignals();
    void initClient();
    bool connectMainServer();
    void startRecvProtocol();   // 开始接收消息

private slots:
    // task_manager_信号的槽函数函数
    void handleGetFileInfo(std::shared_ptr<std::vector<FileInfo>> vet);
    void handleError(QString message);

    // ui 信号的槽函数
    void handleWidgetChaned();
    void handleGetsClicked(ItemDate item);
    void handlePutsClicked();
    void handleCreateDir(std::uint64_t parent_id, QString new_dir_name);
    void handleDeleteFile(std::uint64_t file_id, QString file_name);

    void handleInfoPBClicked();
    void requestCD();

private:
    std::shared_ptr<UserInfo> user_info_;           // 用户信息
    // 确保声明顺序，由于task_manager_也持有task_queue_，并且task_queue_会使用task_manager_
    // 因此需要先析构task_queue_，在析构task_manager_
    // 因此先声明task_manager_（析构顺序与声明顺序相反）
    std::shared_ptr<ShortTaskManager> task_manager_;// 短任务管理器
    std::shared_ptr<SR_Tool> sr_tool_;              // 发送和接受数据工具
    std::shared_ptr<TaskQue> task_queue_;           // 任务队列


    QString cur_server_ip_;     // 连接主服务器后，当前分配的服务器
    int cur_s_port_ = 0;        // 当前分配的短任务端口
    int cur_l_port_ = 0;        // 当前分配的长任务端口

// ui
private:
    QStackedWidget *show_widget_sw_{ nullptr }; // 用于显示当前窗口的 StackedWidget
    // FileViewSystem *file_view_{ nullptr };      // 文件系统视图
    FileSystem *file_view_{ nullptr };
    QWidget *file_trans_wd_{ nullptr };         // 文件传输视图
    QScrollArea *file_trans_sa_{ nullptr };     // 用于显示文件传输列表的 ScrollArea

    QPushButton *file_pb_{ nullptr };           // 显示文件窗口的按钮
    QLabel *file_lb_{ nullptr };
    QPushButton *trans_pb_{ nullptr };          // 显示传输列表窗口的按钮
    QLabel *trans_lb_{ nullptr };
    QPushButton *user_info_pb_{ nullptr };      // 显示个人信息窗口的按钮
    QLabel *user_info_lb_{ nullptr };
};
#endif // DISKCLIENT_H
