#ifndef TPROGRESS_H
#define TPROGRESS_H

#include "DisallowCopyAndMove.h"
#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <atomic>
#include <mutex>
#include <condition_variable>

class TProgress : public QDialog {
    Q_OBJECT

public:
    // 禁止拷贝和移动
    DISALLOW_COPY_AND_MOVE(TProgress);

    TProgress(QWidget *parent = nullptr);
    ~TProgress() = default;

public:
    void initUI();
    void initSignals();
    void applyQSS();    // 使用QSS美化控件

    bool setFileName(const std::int32_t option, const QString& file_name);
    std::shared_ptr<std::atomic<std::uint32_t>> getAtomic();
    std::shared_ptr<std::condition_variable> getCv();
    std::shared_ptr<std::mutex> getMutex();

signals:    // 用于控制下载的请求信号（绑定到DownTool的槽函数，发送控制请求）
    void pause();
    void resume();
    void cancel();

public slots:
    void handleUpdateProgress(std::int64_t bytes, std::int64_t total); // 更新下载和上传进度
    void handleFinished();  // 进度达到100%槽函数

private slots:  // ui槽函数
    void handleClosePBClicked();     // 结束键按下
    void handleContinuePBClicked();  // 继续键按下
    void handleStopPBClicked();      // 暂停键按下

private:    // 成员变量
    QString file_name_;         // 文件名
    std::int32_t option_{ 0 };  // 区分上传下载，1上传，2下载
    // 控制上传下载
    std::shared_ptr<std::atomic<std::uint32_t>> control_;
    std::shared_ptr<std::condition_variable> cv_;
    std::shared_ptr<std::mutex> mutex_;

private:    // ui
    QLabel *file_lb_{ nullptr };
    QProgressBar *progress_pg_{ nullptr };
    QPushButton *close_pb_{ nullptr };
    QPushButton *continue_pb_{ nullptr };
    QPushButton *stop_pb_{ nullptr };
};

#endif // TPROGRESS_H
