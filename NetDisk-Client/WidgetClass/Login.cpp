#include "Login.h"
#include "Serializer.h"
#include "BufferPool.h"
#include <QDebug>


Login::Login(std::shared_ptr<SR_Tool> sr_tool, UserInfo *info, QWidget *parent)
    :  QDialog(parent), sr_tool_(sr_tool), is_sign_(false), is_save_info_(false), info_(info)
{
    initUI();
    initSignals();

    this->setWindowTitle("登录");
}

Login::~Login() {
    if (sr_tool_) {
        sr_tool_->SR_stop();    // 停止异步任务
    }
    // 断开 sr_tool_ 信号
    disconnect(sr_tool_.get(), &SR_Tool::connected,
            this, &Login::handleConnected);
    disconnect(sr_tool_.get(), &SR_Tool::sendOK,
            this, &Login::handleSend);
    disconnect(sr_tool_.get(), &SR_Tool::error,
            this, &Login::handleError);
    disconnect(sr_tool_.get(), &SR_Tool::recvPDURespondOK,
            this, &Login::handleRecvPDURespond);

    // 断开按钮信号
    disconnect(signin_pb_, &QPushButton::clicked,
            this, &Login::handleSignInPBClicked);
    disconnect(signup_pb_, &QPushButton::clicked,
            this, &Login::handleSignUpPBClicked);
}

// 初始化UI
void Login::initUI() {
    // 初始化控件
    QFont lb_ft;
    lb_ft.setPointSize(16);
    QFont le_ft;
    le_ft.setPointSize(16);
    QFont pb_ft;
    pb_ft.setPointSize(14);

    user_lb_ = new QLabel("账号: ", this);
    pwd_lb_ = new QLabel("密码: ", this);
    user_le_ = new QLineEdit(this);
    pwd_le_ = new QLineEdit(this);
    signin_pb_ = new QPushButton("登录", this);
    signup_pb_ = new QPushButton("注册", this);

    // 设置控件属性
    user_lb_->setFont(lb_ft);
    pwd_lb_->setFont(lb_ft);
    user_le_->setFont(le_ft);
    pwd_le_->setFont(le_ft);
    signin_pb_->setFont(pb_ft);
    signup_pb_->setFont(pb_ft);

    pwd_le_->setEchoMode(QLineEdit::Password);  // 隐藏密码

    // 布局
    // 用户标签和输入框合成一个水平布局
    QHBoxLayout* user_hbl = new QHBoxLayout;
    user_hbl->addWidget(user_lb_);
    user_hbl->addWidget(user_le_);
    // 密码标签和输入框合成一个水平布局
    QHBoxLayout* pwd_hbl = new QHBoxLayout;
    pwd_hbl->addWidget(pwd_lb_);
    pwd_hbl->addWidget(pwd_le_);
    // user_hbl 和 pwd_hbl 合成一个垂直布局
    QVBoxLayout* user_pwd_vbl = new QVBoxLayout;
    user_pwd_vbl->addLayout(user_hbl);
    user_pwd_vbl->addLayout(pwd_hbl);
    // 登录注册按钮合成水平布局
    QHBoxLayout* pb_hbl = new QHBoxLayout;
    pb_hbl->addWidget(signin_pb_);
    pb_hbl->addWidget(signup_pb_);
    // 输入和按钮合成一个垂直布局
    QVBoxLayout* main_vbl = new QVBoxLayout;
    main_vbl->addLayout(user_pwd_vbl);
    main_vbl->addLayout(pb_hbl);

    // 应用布局
    setLayout(main_vbl);

}

// 连接信号和槽
void Login::initSignals() {
    // 连接信号和槽
    // 连接 sr_tool_的信号
    connect(sr_tool_.get(), &SR_Tool::connected,
            this, &Login::handleConnected);
    connect(sr_tool_.get(), &SR_Tool::sendOK,
            this, &Login::handleSend);
    connect(sr_tool_.get(), &SR_Tool::error,
            this, &Login::handleError);
    connect(sr_tool_.get(), &SR_Tool::recvPDURespondOK,
            this, &Login::handleRecvPDURespond);

    // 连接按钮信号
    connect(signin_pb_, &QPushButton::clicked,
            this, &Login::handleSignInPBClicked);
    connect(signup_pb_, &QPushButton::clicked,
            this, &Login::handleSignUpPBClicked);
}

// 发送登录请求
void Login::signin() {
    PDU pdu;
    pdu.header.type = ProtocolType::PDU_TYPE;
    pdu.header.body_len = PDU_BODY_BASE_LEN;
    pdu.code = Code::SIGNIN;
    memcpy(pdu.user, user_byte_.data(), user_byte_.size());
    memcpy(pdu.pwd, pwd_byte_.data(), pwd_byte_.size());
    pdu.msg_len = 0;

    // 序列化pdu
    auto buf = Serializer::serialize(pdu);
    qDebug() << "serialize success";
    // 发送请求
    sr_tool_->asyncSend(buf, PROTOCOLHEADER_LEN + pdu.header.body_len);
    qDebug() << "asyncSend signup success";
}

// 发送注册请求
void Login::signup() {
    PDU pdu;
    pdu.header.type = ProtocolType::PDU_TYPE;
    pdu.header.body_len = PDU_BODY_BASE_LEN;
    pdu.code = Code::SIGNUP;
    memcpy(pdu.user, user_byte_.data(), user_byte_.size());
    memcpy(pdu.pwd, pwd_byte_.data(), pwd_byte_.size());
    pdu.msg_len = 0;

    // 序列化pdu
    auto buf = Serializer::serialize(pdu);
    // 发送请求
    sr_tool_->asyncSend(buf, PROTOCOLHEADER_LEN + pdu.header.body_len);
}

// 连接服务器
void Login::connection() {
    if (connected_) {   // 已连接
        handleConnected();  // 执行连接成功函数
    }
    else {  // 未连接
        sr_tool_->asyncConnect();
    }

    sr_tool_->SR_run();     // 开始异步任务
}

// 连接服务器成功的处理函数
void Login::handleConnected() {
    connected_ = true;

    qDebug() << "connect server success";

    if (in_or_up_) {
        signin();
    }
    else {
        signup();
    }
}

// 发送信息成功的处理函数
void Login::handleSend() {
    // 接收服务器的回复
    qDebug() << "async send success";
    auto buf = BufferPool::getInstance().acquire(); // 获取缓冲区
    sr_tool_->asyncRecvProtocol(false); // 接收通信协议

}

// SR_Tool发生错误的处理函数
void Login::handleError(QString message) {
    QMessageBox::warning(this, "错误", message);
}

// SR_Tool接收到RespondPack的处理函数
void Login::handleRecvPDURespond(std::shared_ptr<PDURespond> pdu) {
    qDebug() << "async recv PDURespond success";

    if (pdu->code != Code::SIGNIN && pdu->code != Code::SIGNUP) {
        QMessageBox::warning(this, "操作异常", "不是预期的操作");
        return;
    }
    if (Status::SUCCESS == pdu->status) {
        // 保存用户信息
        if (pdu->msg.size() >= USERSCOLLEN*USERSCOLMAXSIZE) {
            char* ptr = pdu->msg.data();
            size_t offset = 0;
            memcpy(info_->user,          ptr + offset, USERSCOLMAXSIZE);
            offset += USERSCOLMAXSIZE;
            memcpy(info_->pwd,           ptr + offset, USERSCOLMAXSIZE);
            offset += USERSCOLMAXSIZE;
            memcpy(info_->cipher,        ptr + offset, USERSCOLMAXSIZE);
            offset += USERSCOLMAXSIZE;
            memcpy(info_->is_vip,        ptr + offset, USERSCOLMAXSIZE);
            offset += USERSCOLMAXSIZE;
            memcpy(info_->capacity_sum,  ptr + offset, USERSCOLMAXSIZE);
            offset += USERSCOLMAXSIZE;
            memcpy(info_->used_capacity, ptr + offset, USERSCOLMAXSIZE);
            offset += USERSCOLMAXSIZE;
            memcpy(info_->salt,          ptr + offset, USERSCOLMAXSIZE);
            offset += USERSCOLMAXSIZE;
            memcpy(info_->vip_date,      ptr + offset, USERSCOLMAXSIZE);

            QDialog::accept();      // 返回 QDialog::Accepted
        }
        else {
            QMessageBox::warning(this, "登录失败", "接收用户数据失败");
        }
    }
    else {
        if (in_or_up_) {
            QMessageBox::warning(this, "登录失败", "用户名或密码错误");
        }
        else {
            QMessageBox::warning(this, "注册失败", "用户已存在");
        }
    }
    sr_tool_->SR_stop();    // 停止异步操作
}

// 点击 signin_pb_ 的处理函数
void Login::handleSignInPBClicked() {
    in_or_up_ = true;
    user_ = user_le_->text();
    user_byte_ = user_.toUtf8();
    pwd_ = pwd_le_->text();
    pwd_byte_ = pwd_.toUtf8();

    qDebug() << "signin user:" << user_ << "pwd:" << pwd_;

    connection();
}

void Login::handleSignUpPBClicked() {
    in_or_up_ = false;
    user_ = user_le_->text();
    user_byte_ = user_.toUtf8();
    pwd_ = pwd_le_->text();
    pwd_byte_ = pwd_.toUtf8();

    qDebug() << "signup user:" << user_ << "pwd:" << pwd_;

    connection();
}


