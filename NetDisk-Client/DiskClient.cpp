#include "DiskClient.h"
#include "UdTool.h"
#include "TProgress.h"
#include "UserInfoWidget.h"
#include "Login.h"
#include "DownTool.h"
#include "Serializer.h"
#include "BufferPool.h"
#include <QThread>
#include <QFileDialog>
#include <QDebug>

DiskClient::DiskClient(QWidget *parent)
    : QWidget(parent)
    , user_info_(std::make_shared<UserInfo>())
    , task_queue_(std::make_shared<TaskQue>(1))
{
    // sr_tool_的创建需要 ip 和端口，在连接负载均衡器后获取
    // 因此sr_tool_在连接负载均衡器（connectMainServer)后创建

    // task_manager_的创建需要sr_tool_
}

DiskClient::~DiskClient() {
    // 停止异步操作
    if (sr_tool_) {
        sr_tool_->SR_stop();
    }

    // 关闭线程池
    task_queue_->close();

    // 断开连接
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

bool DiskClient::startClient() {
    // 连接主服务器
    bool ret = connectMainServer(); // 在这个过程中创建 sr_tool_
    bool res_login = false;
    if (ret) {
        // 连接成功，加载登录界面
        Login login(sr_tool_, user_info_.get(), this);
        int ret = login.exec();     // 应用级模态显示
        if (QDialog::Accepted == ret) { // 登录成功
            res_login = true;
        }
    }
    // login销毁后，再调用初始化客户端的相关函数（因为login连接了sr_tool_信号，会造成干扰）
    if (res_login) {
        initClient();   // 初始化UI，在这里创建 task_manager_
        startRecvProtocol();    // 开始接收服务端的信息
        requestCD();    // 请求文件列表，构建文件系统
        return true;
    }
    return false;
}

// 初始化 UI
void DiskClient::initUI() {
    // 初始化控件
    QFont lb_ft;
    lb_ft.setPointSize(16);
    QIcon file_icon(":/icon/icon/FileWidget.svg");
    QIcon trans_icon(":/icon/icon/TranWidget.svg");
    QIcon user_info_icon(":/icon/icon/UserInfo.svg");

    show_widget_sw_ = new QStackedWidget(this);
    // file_view_ = new FileViewSystem(this);
    file_view_ = new FileSystem(this);
    file_trans_wd_ = new QWidget(this);
    file_trans_sa_ = new QScrollArea(this);
    file_pb_ = new QPushButton(this);
    file_lb_ = new QLabel("文件", this);
    trans_pb_ = new QPushButton(this);
    trans_lb_ = new QLabel("传输", this);
    user_info_pb_ = new QPushButton(this);
    user_info_lb_ = new QLabel("个人信息", this);

    // 设置控件属性
    show_widget_sw_->addWidget(file_view_);
    show_widget_sw_->addWidget(file_trans_sa_);

    QVBoxLayout* file_trans_vbl = new QVBoxLayout;
    file_trans_vbl->setAlignment(Qt::AlignTop);     // 顶部对其
    file_trans_wd_->setLayout(file_trans_vbl);

    file_trans_sa_->setWidgetResizable(true);       // 自动调整内部widget大小
    file_trans_sa_->setAlignment(Qt::AlignRight);
    file_trans_sa_->setWidget(file_trans_wd_);

    file_pb_->setObjectName("file_pb_");
    file_pb_->setIcon(file_icon);
    file_pb_->setIconSize(QSize(64, 64));
    file_lb_->setFont(lb_ft);
    file_lb_->setAlignment(Qt::AlignCenter);        // 文本剧中对齐

    trans_pb_->setObjectName("trans_pb_");
    trans_pb_->setIcon(trans_icon);
    trans_pb_->setIconSize(QSize(64, 64));
    trans_lb_->setFont(lb_ft);
    trans_lb_->setAlignment(Qt::AlignCenter);       // 文本剧中对齐

    user_info_pb_->setIcon(user_info_icon);
    user_info_pb_->setIconSize(QSize(64, 64));
    user_info_lb_->setFont(lb_ft);
    user_info_lb_->setAlignment(Qt::AlignCenter);   // 文本剧中对齐

    // 设置布局
    // 文件按钮和标签合成垂直布局
    QVBoxLayout* file_vbl = new QVBoxLayout;
    file_vbl->addWidget(file_pb_, 0, Qt::AlignCenter);
    file_vbl->addWidget(file_lb_, 0, Qt::AlignCenter);

    // 文件按钮和标签合成垂直布局
    QVBoxLayout* trans_vbl = new QVBoxLayout;
    trans_vbl->addWidget(trans_pb_, 0, Qt::AlignCenter);
    trans_vbl->addWidget(trans_lb_, 0, Qt::AlignCenter);

    // 用户信息按钮和标签合成垂直布局
    QVBoxLayout* user_info_vbl = new QVBoxLayout;
    user_info_vbl->addWidget(user_info_pb_, 0, Qt::AlignCenter);
    user_info_vbl->addWidget(user_info_lb_, 0, Qt::AlignCenter);

    // 三个按钮合成垂直布局
    QVBoxLayout* pb_vbl = new QVBoxLayout;
    pb_vbl->addLayout(file_vbl);
    pb_vbl->addLayout(trans_vbl);
    // 添加弹簧
    pb_vbl->addItem(new QSpacerItem(0, 64, QSizePolicy::Minimum, QSizePolicy::Expanding));
    pb_vbl->addLayout(user_info_vbl);

    // 按钮和show_widget_sw_合成水平布局
    QHBoxLayout* main_hbl = new QHBoxLayout;
    main_hbl->addLayout(pb_vbl);
    main_hbl->addWidget(show_widget_sw_);

    // 应用布局
    setLayout(main_hbl);
    resize(QSize(800, 600));

    // 应用QSS
    applyQSS();
}

void DiskClient::applyQSS() {
    file_pb_->setStyleSheet(R"(
        QPushButton {
            border: none;         /* 移除边框 */
            background: transparent;  /* 背景透明 */
            padding: 0px;         /* 移除内边距（避免图标周围留白） */
        }
        QPushButton:hover {
            background: rgba(200, 200, 200, 50);  /* 鼠标悬停时轻微背景（可选） */
        }
        QPushButton:pressed {
            background: rgba(150, 150, 150, 50);  /* 按下时背景（可选） */
        }
    )");

    trans_pb_->setStyleSheet(R"(
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

    user_info_pb_->setStyleSheet(R"(
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

// 初始化信号和槽函数
void DiskClient::initSignals() {
    // 短任务管理器，用于处理短任务
    task_manager_ = std::make_shared<ShortTaskManager>(sr_tool_, task_queue_, this);

    // 连接task_manager信号
    // 获取文件列表成功信号
    connect(task_manager_.get(), &ShortTaskManager::getFileListOK,
            this, &DiskClient::handleGetFileInfo);
    // 创建文件夹成功信号
    // connect(task_manager_.get(), &ShortTaskManager::createDirOK,
    //         file_view_, &FileViewSystem::addNewDirItem);
    connect(task_manager_.get(), &ShortTaskManager::createDirOK,
            file_view_, &FileSystem::addNewDirItem);
    // 删除文件成功信号
    connect(task_manager_.get(), &ShortTaskManager::delFileOK,
            this, &DiskClient::requestCD);
    // 错误信号
    connect(task_manager_.get(), &ShortTaskManager::error,
            this, &DiskClient::handleError);

    // 连接UI信号
    // 点击文件按钮信号
    connect(file_pb_, &QPushButton::clicked,
            this, &DiskClient::handleWidgetChaned);
    // 点击传输按钮信号
    connect(trans_pb_, &QPushButton::clicked,
            this, &DiskClient::handleWidgetChaned);
    // 点击用户信息按钮信号
    connect(user_info_pb_, &QPushButton::clicked,
            this, &DiskClient::handleInfoPBClicked);
    // 刷新视图信号
    // connect(file_view_, &FileViewSystem::refreshView,
    //         this, &DiskClient::requestCD);
    connect(file_view_, &FileSystem::refreshView,
            this, &DiskClient::requestCD);
    // 下载信号
    // connect(file_view_, &FileViewSystem::download,
    //         this, &DiskClient::handleGetsClicked);
    connect(file_view_, &FileSystem::download,
            this, &DiskClient::handleGetsClicked);
    // 上传信号
    // connect(file_view_, &FileViewSystem::upload,
    //         this, &DiskClient::handlePutsClicked);
    connect(file_view_, &FileSystem::upload,
            this, &DiskClient::handlePutsClicked);
    // 创建文件夹信号
    // connect(file_view_, &FileViewSystem::createDir,
    //         this, &DiskClient::handleCreateDir);
    connect(file_view_, &FileSystem::createDir,
            this, &DiskClient::handleCreateDir);
    // 删除文件信号
    // connect(file_view_, &FileViewSystem::deleteFile,
    //         this, &DiskClient::handleDeleteFile);
    connect(file_view_, &FileSystem::deleteFile,
            this, &DiskClient::handleDeleteFile);
}

// 初始化客户端
void DiskClient::initClient() {
    initUI();
    initSignals();

    show_widget_sw_->setCurrentWidget(file_view_);
}

// 连接主服务器（负载均衡器），获得分配的地址和端口
bool DiskClient::connectMainServer() {
    // 构建主服务器端点
    boost::asio::ip::tcp::endpoint _endpoint(boost::asio::ip::make_address(MainServerIP), MainServerPort);
    boost::asio::io_context _io_context;    // 构建IO上下文
    boost::asio::ip::tcp::socket _socket(_io_context);
    boost::system::error_code ec;
    _socket.connect(_endpoint, ec);     // 连接到负载均衡器
    if (ec) {   //连接负载均衡器失败，使用默认地址
        qDebug() << "均衡器无法连接";
    }
    else {
        try {
            ServerInfoPack server_info;
            auto info_buf = BufferPool::getInstance().acquire();
            // 接收服务器信息
            boost::asio::read(_socket, boost::asio::buffer(info_buf.get(), PROTOCOLHEADER_LEN + SERVERINFOPACK_BODY_LEN));
            _socket.close(); // 关闭连接
            // 反序列化
            if (!Serializer::deserialize(info_buf.get(), PROTOCOLHEADER_LEN + SERVERINFOPACK_BODY_LEN, server_info)) {
                throw std::runtime_error("反序列化ServerInfoPack失败");
            }
            // 保存服务器信息
            cur_server_ip_ = server_info.ip;
            cur_s_port_ = server_info.sport;
            cur_l_port_ = server_info.lport;

            qDebug() << "连接服务器:" << server_info.name << "ip:" << cur_server_ip_ << "sport:" << cur_s_port_ << "lport" << cur_l_port_;

            if (!cur_server_ip_.isEmpty()) {
                // 构建收发工具
                // !!!!!!!!!!!!!!!!!!!!!! 疑问：已经使用了智能指针为什么还要设置父对象，不会冲突吗 !!!!!!!!!!!!!!!!!!!!!!!!!!
                sr_tool_ = std::make_shared<SR_Tool>(cur_server_ip_.toStdString(), cur_s_port_, this);
                return true;
            }
        }
        catch (std::exception& e) {
            qDebug() << "均衡器无法连接，使用默认服务器:" << e.what();
        }
    }
    // 连接默认服务器
    cur_server_ip_ = DefaultServerIP;
    cur_s_port_ = DefaultServerSPort;
    cur_l_port_ = DefaultServerLPort;
    sr_tool_ = std::make_shared<SR_Tool>(cur_server_ip_.toStdString(), cur_s_port_, this);  // 构建收发工具

    qDebug() << "连接默认服务器ip:" << cur_server_ip_ << "sport:" << cur_s_port_ << "lport" << cur_l_port_;
    return true;
}

void DiskClient::startRecvProtocol() {
    if (sr_tool_) {
        sr_tool_->asyncRecvProtocolContinue();
        sr_tool_->SR_run();
        qDebug() << "start async loop";
    }
}

// 处理 task_manager_ 的 ShortTaskManager::getFileListOK 信号的槽函数
void DiskClient::handleGetFileInfo(std::shared_ptr<std::vector<FileInfo>> vet) {
    file_view_->initView(*vet);
}

// 处理 task_manager_ 的 ShortTaskManager::error 信号的槽函数
void DiskClient::handleError(QString message) {
    QMessageBox::warning(this, "Error", message.toUtf8());
}

// 处理 file_pb_ 和 trans_pb_ 的 &QPushButton::clicked 信号的槽函数
void DiskClient::handleWidgetChaned() {
    QPushButton *pb = static_cast<QPushButton*>(sender());

    QString pb_name = pb->objectName();
    if (pb_name == "file_pb_") {
        show_widget_sw_->setCurrentIndex(0);
    }
    else if (pb_name == "trans_pb_") {
        show_widget_sw_->setCurrentIndex(1);
    }
}

// 处理 file_view_ 的 FileViewSystem::download 信号的槽函数
void DiskClient::handleGetsClicked(ItemDate item) {
    uint64_t file_id = item.id;

    // 用于QFileDialog的过滤：filter = "文件类型(*.${拓展名})"
    QString filter ="文件类型(*"+ item.file_name.right(item.file_name.size() - item.file_name.lastIndexOf("."))+")";

    // !!!!!!!!!!!!!!!!!!!!! 可改为用户设置的默认路径 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // 创建一个对话框打开文件，并获取文件的下载路径
    QString file_path = QFileDialog::getSaveFileName(this, "选择保存的文件", item.file_name, filter);
    if (file_path.isEmpty()) {
        return;
    }

    // 构建通信pdu
    TranPdu pdu;
    pdu.header.type = ProtocolType::TRANPDU_TYPE;
    pdu.header.body_len = TRANPDU_BODY_LEN;
    pdu.tran_pdu_code = Code::GETS;
    // 这里用parent_dir_id字段保存file_id，注意在服务端识别
    pdu.parent_dir_id = file_id;
    memcpy(pdu.user, user_info_->user, sizeof(pdu.user));
    memcpy(pdu.pwd, user_info_->pwd, sizeof(pdu.pwd));
    QByteArray file_name = QFileInfo(file_path).fileName().toUtf8();
    memcpy(pdu.file_name, file_name.data(), file_name.size());
    pdu.file_name[file_name.size()] = '\0';

    pdu.sended_size = 0;
    pdu.file_size = item.file_size;

    // 构建进度条
    TProgress* progress = new TProgress(this);
    progress->setFileName(2, file_path);    // 2 表示下载
    file_trans_wd_->layout()->addWidget(progress);  // 添加到文件传输列表

    // 准备下载
    // !!!!!!!!!!!!!!!!!!!!!!!! 疑问：能确保thread和worker的资源什么情况下都会被销毁吗 !!!!!!!!!!!!!!!!!!
    QThread* thread = new QThread();
    DownTool* worker = new DownTool(cur_server_ip_, cur_l_port_, pdu, file_path, nullptr);  // 创建下载工具

    // 连接信号
    // worker完成时，删除worker，并关闭线程
    QObject::connect(worker, &DownTool::workFinished,
                     thread, &QThread::quit);       // quit会发射finished信号
    QObject::connect(worker, &DownTool::workFinished,
                     worker, &DownTool::deleteLater);
    // worker出现错误时，删除worker，并关闭线程
    QObject::connect(worker, &DownTool::error,
                     thread, &QThread::quit);
    QObject::connect(worker, &DownTool::error,
                     worker, &DownTool::deleteLater);
    // 线程完成时，删除线程
    QObject::connect(thread, &QThread::finished,
                     thread, &QThread::deleteLater);
    // 线程run（执行QThread::exec()）前，执行worker的下载任务
    QObject::connect(thread, &QThread::started,
                     worker, &DownTool::doingDown);

    // 如果使用阻塞连接会导致发送线程阻塞，影响发送效率。可以减少发送次数避免这个问题。
    // 修改进度条
    QObject::connect(worker, &DownTool::sendProgress,
                     progress, &TProgress::handleUpdateProgress);
    QObject::connect(worker, &DownTool::workFinished,
                     progress, &TProgress::handleFinished);
    // 进度条控制下载状态
    QObject::connect(progress, &TProgress::pause,
                     worker, &DownTool::sendPauseRequest);
    QObject::connect(progress, &TProgress::resume,
                     worker, &DownTool::sendResumeRequest);
    QObject::connect(progress, &TProgress::cancel,
                     worker, &DownTool::sendCancelRequest);
    // 处理错误
    QObject::connect(worker, &DownTool::error,
                     this, &DiskClient::handleError);


    // 使用moveToThread会转换worker的线程亲和性，使得worker的槽函数在转移到的线程执行
    worker->moveToThread(thread);
    // 在这里不使用线程池，是因为要使用Qt的信号槽机制，所以使用Qt提供的线程
    thread->start();
    show_widget_sw_->setCurrentIndex(1);    // 设置当前页面为传输页面
}

// 处理 file_view_ 的 FileViewSystem::upload信号的槽函数
void DiskClient::handlePutsClicked() {
    // 获取当前目录id（上传文件到当前目录）
    uint64_t parent_id = file_view_->getCurDirId();
    // 创建一个对话框打开文件，获取要上传文件的路径
    QString file_path = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath(), "所有文件(*.*)");
    if (file_path.isEmpty()) {
        return;
    }

    // 判断所选文件是否合法
    // 对文件长度进行判断，服务端不接受太长的文件名
    QFileInfo file_info(file_path);
    // !!!!!!!!!!!!!!!!!!!!!!! 这里直接使用 100，不利于维护和更新，后续可使用宏 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (file_info.fileName().toUtf8().size() >= 100) {
        QMessageBox::warning(this, "警告", "文件名过长");
        return;
    }

    // 以 d 结尾的服务端默认为文件夹，所有不接受
    QString suffix = file_info.suffix();
    if (suffix == "d") {
        QMessageBox::warning(this, "提示", "后缀名不能为d");
        return;
    }

    QByteArray file_path_bytes = file_path.toUtf8();

    // 构建通信pdu
    TranPdu pdu;
    pdu.header.type = ProtocolType::TRANPDU_TYPE;
    pdu.header.body_len = TRANPDU_BODY_LEN;
    pdu.tran_pdu_code = Code::PUTS;
    pdu.parent_dir_id = parent_id;
    memcpy(pdu.user, user_info_->user, sizeof(pdu.user));
    memcpy(pdu.pwd, user_info_->pwd, sizeof(pdu.pwd));
    memcpy(pdu.file_name, file_path_bytes.data(), file_path_bytes.size());

    // 构建进度条
    TProgress* progress = new TProgress(this);
    progress->setFileName(1, file_path);    // 1 表示上传
    file_trans_wd_->layout()->addWidget(progress);

    // !!!!!!!!!!!!!!!!!!! 疑问：能确保thread和worker在任何情况下都能释放资源吗 !!!!!!!!!!!!!!!!!!!!!
    // 上传任务在其它线程完成，之所以使用QThread，而不是线程池，是因为要用到Qt的信号槽机制
    QThread* thread = new QThread();
    UdTool* worker = new UdTool(cur_server_ip_, cur_l_port_, nullptr);  // 创建上传工具
    worker->setTranPdu(pdu);    // 设置要上传文件的信息

    // 连接信号
    // worker完成时，删除worker，并关闭线程
    QObject::connect(worker, &UdTool::workFinished,
                     thread, &QThread::quit);   // quit会触发finished信号，并清除线程资源
    QObject::connect(worker, &UdTool::workFinished,
                     worker, &UdTool::deleteLater);
    // worker出现错误时，删除worker，并关闭线程
    QObject::connect(worker, &UdTool::error,
                     thread, &QThread::quit);   // quit会触发finished信号，并清除线程资源
    QObject::connect(worker, &UdTool::error,
                     worker, &UdTool::deleteLater);
    // 线程完成时，删除线程
    QObject::connect(thread, &QThread::finished,
                     thread, &QThread::deleteLater);    // 删除线程本身
    // 线程run（执行QThread::exec()）前，执行worker的上传任务
    QObject::connect(thread, &QThread::started,
                     worker, &UdTool::doingUp);

    // 如果使用阻塞连接会导致发送线程阻塞，影响发送效率。可以减少发送次数避免这个问题。
    // 修改进度条
    QObject::connect(worker, &UdTool::sendProgress,
                     progress, &TProgress::handleUpdateProgress);
    QObject::connect(worker, &UdTool::workFinished,
                     progress, &TProgress::handleFinished);
    // 进度条点击取消任务
    QObject::connect(progress, &TProgress::cancel,
                     worker, &UdTool::sendCancelRequest);
    // 上传完成，添加文件项
    // QObject::connect(worker, &UdTool::sendItemData,
    //                  file_view_, &FileViewSystem::addNewFileItem);
    QObject::connect(worker, &UdTool::sendItemData,
                     file_view_, &FileSystem::addNewFileItem);
    // 处理错误
    QObject::connect(worker, &UdTool::error,
                     this, &DiskClient::handleError);
    // QObject::connect(worker, &UdTool::workFinished,
    //                  this, &DiskClient::RequestCD);    // 上传完成，刷新界面。文件太多会导致刷新压力大，不使用。

    // 设置worker的控制变量，与progress共用一套，因为progress可以控制worker的行为
    worker->setControlAtomic(progress->getAtomic(), progress->getCv(), progress->getMutex());

    // 启动线程
    worker->moveToThread(thread);
    thread->start();
    show_widget_sw_->setCurrentIndex(1);  // 切换到文件传输页面
}

// 处理 file_view_ 的 FileViewSystem::createDir 信号的槽函数
void DiskClient::handleCreateDir(uint64_t parent_id, QString new_dir_name) {
    task_manager_->makeDir(parent_id, new_dir_name);
}

// 处理 file_view_ 的 FileViewSystem::deleteFile 信号的槽函数
void DiskClient::handleDeleteFile(uint64_t file_id, QString file_name) {
    task_manager_->delFile(file_id, file_name);
}

// 处理 user_info_pb_ 的 QPushButton::clicked 信号的槽函数
void DiskClient::handleInfoPBClicked() {
    // 创建用户信息界面
    UserInfoWidget *user_dialog = new UserInfoWidget(user_info_.get(), this);
    user_dialog->setAttribute(Qt::WA_DeleteOnClose);
    user_dialog->setWindowTitle("用户信息");
    user_dialog->show();
}

// 向服务器请求读取文件列表
void DiskClient::requestCD() {
    task_manager_->getFileList();
}
