#include "TProgress.h"
#include <climits>
#include <QMessageBox>

TProgress::TProgress(QWidget *parent)
    : QDialog(parent), option_(0)
    , control_(std::make_shared<std::atomic<std::uint32_t>>(0))
    , cv_(std::make_shared<std::condition_variable>())
    , mutex_(std::make_shared<std::mutex>())
{
    initUI();
    initSignals();
}

// 初始化UI
void TProgress::initUI() {
    // 初始化控件
    QFont lb_ft;
    lb_ft.setPointSize(10);
    QFont pg_ft;
    pg_ft.setPointSize(10);
    QIcon close_icon(":/icon/icon/TranEnd.svg");
    QIcon continue_icon(":/icon/icon/TranStart.svg");
    QIcon stop_icon(":/icon/icon/TranStop.svg");

    file_lb_        = new QLabel(this);
    file_lb_->setFont(lb_ft);
    progress_pg_    = new QProgressBar(this);
    progress_pg_->setFont(pg_ft);
    progress_pg_->setMinimumHeight(20);  // 最小高度
    progress_pg_->setAlignment(Qt::AlignCenter);    // 文本居中对其

    close_pb_       = new QPushButton(this);
    close_pb_->setIcon(close_icon);
    close_pb_->setMinimumWidth(30);
    close_pb_->setMaximumWidth(60);
    continue_pb_    = new QPushButton(this);
    continue_pb_->setIcon(continue_icon);
    continue_pb_->setMinimumWidth(30);
    continue_pb_->setMaximumWidth(60);
    stop_pb_        = new QPushButton(this);
    stop_pb_->setIcon(stop_icon);
    stop_pb_->setMinimumWidth(30);
    stop_pb_->setMaximumWidth(60);

    // 布局
    // 按钮合成一个水平布局
    QHBoxLayout *pb_hbl = new QHBoxLayout;
    pb_hbl->addWidget(close_pb_);
    pb_hbl->addWidget(continue_pb_);
    pb_hbl->addWidget(stop_pb_);
    pb_hbl->setSpacing(5);  // 按钮之间就间距尽量小点
    // 进度条和按钮合成水平布局，进度条占3份，按钮占一份
    QHBoxLayout *pg_hbl = new QHBoxLayout;
    pg_hbl->addWidget(progress_pg_, 3);
    pg_hbl->addLayout(pb_hbl, 1);
    // 文件标题和进度条和成垂直布局
    QVBoxLayout *main_vbl = new QVBoxLayout;
    main_vbl->addWidget(file_lb_);
    main_vbl->addLayout(pg_hbl);

    // 应用布局
    setLayout(main_vbl);

    // 美化
    applyQSS();
}

// 初始化信号和槽函数
void TProgress::initSignals() {

    // 初始化按钮槽函数
    connect(close_pb_, &QPushButton::clicked,
            this, &TProgress::handleClosePBClicked);
    connect(continue_pb_, &QPushButton::clicked,
            this, &TProgress::handleContinuePBClicked);
    connect(stop_pb_, &QPushButton::clicked,
            this, &TProgress::handleStopPBClicked);
}

void TProgress::applyQSS() {
    // 进度条
    progress_pg_->setStyleSheet(R"(
        QProgressBar {
            border: 2px solid #5c5c5c;
            border-radius: 4px;
            height: 20px;  /* 直接在样式表中指定高度 */
            text-align: center;
            font-size: 14pt;  /* 百分比文字大小 */
        }
        QProgressBar::chunk {
            background-color: #4cd964;
            border-radius: 2px;  /* 进度块圆角 */
        }
    )");

    // 关闭上传下载按钮
    close_pb_->setStyleSheet(R"(
        QPushButton {
            border: none;         /* 移除边框 */
            background: transparent;  /* 背景透明 */
            padding: 0px;         /* 移除内边距（避免图标周围留白） */
        }
        QPushButton:hover {
            background: rgba(200, 200, 200, 50);  /* 鼠标悬停时轻微背景 */
        }
        QPushButton:pressed {
            background: rgba(150, 150, 150, 50);  /* 按下时背景 */
        }
    )");

    // 继续上传下载按钮
    continue_pb_->setStyleSheet(R"(
        QPushButton {
            border: none;         /* 移除边框 */
            background: transparent;  /* 背景透明 */
            padding: 0px;         /* 移除内边距（避免图标周围留白） */
        }
        QPushButton:hover {
            background: rgba(200, 200, 200, 50);  /* 鼠标悬停时轻微背景 */
        }
        QPushButton:pressed {
            background: rgba(150, 150, 150, 50);  /* 按下时背景 */
        }
    )");

    // 暂停上传下载按钮
    stop_pb_->setStyleSheet(R"(
        QPushButton {
            border: none;         /* 移除边框 */
            background: transparent;  /* 背景透明 */
            padding: 0px;         /* 移除内边距（避免图标周围留白） */
        }
        QPushButton:hover {
            background: rgba(200, 200, 200, 50);  /* 鼠标悬停时轻微背景 */
        }
        QPushButton:pressed {
            background: rgba(150, 150, 150, 50);  /* 按下时背景 */
        }
    )");
}

// 输入上传还是下载，设置文件名。option为1为上传，2为下载
bool TProgress::setFileName(const std::int32_t option, const QString &file_name) {
    file_name_ = file_name;
    option_ = option;
    if (option_ == 1) {
        file_lb_->setText("正在上传：" + file_name);
    }
    else if (option_ == 2) {
        file_lb_->setText("正在下载：" + file_name);
    }
    else {
        file_name_.clear();
        option_ = 0;
        return false;
    }
    return true;
}

// 返回原子控制变量
std::shared_ptr<std::atomic<std::uint32_t>> TProgress::getAtomic() {
    return control_;
}

// 返回条件变量
std::shared_ptr<std::condition_variable> TProgress::getCv() {
    return cv_;
}

// 返回互斥量
std::shared_ptr<std::mutex> TProgress::getMutex() {
    return mutex_;
}

// 更新进度显示
void TProgress::handleUpdateProgress(std::int64_t bytes, std::int64_t total) {
    if (total > INT_MAX) {
        // 按比例缩放
        double ratio = (double)bytes / total;  // 计算比例
        int scale_value = static_cast<int>(ratio * INT_MAX);    // 将比例转为int范围
        progress_pg_->setMaximum(INT_MAX);
        progress_pg_->setValue(scale_value);
    }
    else {
        // 正常处理
        progress_pg_->setMaximum(static_cast<int>(total));
        progress_pg_->setValue(static_cast<int>(bytes));
    }
}

// 进度达到100%槽函数
void TProgress::handleFinished() {
    if (option_ == 1) {
        file_lb_->setText("上传：" + file_name_ + " 成功");
    }
    else if (option_ == 2){
        file_lb_->setText("下载：" + file_name_ + " 成功");
    }
    else {
        file_lb_->setText("未知任务" + file_name_ + "完成");
    }
}

// 结束按钮
void TProgress::handleClosePBClicked() {
    QMessageBox::StandardButton reply;
    // 如果任务未完成，则弹出提示，否则直接结束
    if (progress_pg_->value() != progress_pg_->maximum()) {
        reply = QMessageBox::question(this, "请确认", "确定结束？",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) { // 选择No，什么都不做
            return;
        }
    }

    // 确认取消
    if (option_ == 1) { // 上传：控制control_
        if (2 == control_->load()) {    // 避免重复执行
            return;
        }

        control_->store(2);     // 2 表示关闭
        cv_->notify_one();      // 唤醒工作线程，结束上传或下载
        emit cancel();
    }
    else if (option_ == 2) {    // 下载：发送cancel请求
        emit cancel();
    }

    this->deleteLater();	// 进入事件循环，释放自己，避免资源泄漏
}

// 继续按钮
void TProgress::handleContinuePBClicked() {
    if (option_ == 1) { // 上传：控制control_
        if (0 == control_->load()) {    // 避免重复执行
            return;
        }
        // 如果用户点击过暂停，那么会更改file_lb_，因此需要修改回来
        file_lb_->setText("正在上传：" + file_name_);

        control_->store(0); // 0 表示继续
        cv_->notify_one();  // 唤醒工作线程，继续上传或下载
    }
    else if (option_ == 2) {    // 下载，发送resume请求
        file_lb_->setText("正在下载：" + file_name_);
        emit resume();
    }
}

// 暂停按钮
void TProgress::handleStopPBClicked() {
    if (option_ == 1) { // 上传：控制control_
        if (1 == control_->load()) {    // 避免重复执行
            return;
        }
        file_lb_->setText("已暂停");
        control_->store(1); // 1 表示暂停
    }
    else if (option_ == 2){ // 下载：发送pause请求
        file_lb_->setText("已暂停");
        emit pause();
    }
}



