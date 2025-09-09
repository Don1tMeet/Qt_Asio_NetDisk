#pragma once


#include <QDialog>
#include <QMessageBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "SR_Tool.h"
#include "protocol.h"

class Login : public QDialog
{
    Q_OBJECT

public:
    Login(std::shared_ptr<SR_Tool> sr_tool, UserInfo *info, QWidget *parent = nullptr);
    ~Login();

private:
    void initUI();
    void initSignals();

    void signin();          // 登录
    void signup();          // 注册
    void connection();      // 连接

private slots:
    void handleConnected();
    void handleSend();
    void handleError(QString nessage);
    void handleRecvPDURespond(std::shared_ptr<PDURespond> pdu);

    // ui槽函数
    void handleSignInPBClicked();
    void handleSignUpPBClicked();

private:
    std::shared_ptr<SR_Tool> sr_tool_;  // 用来进行通信
    bool in_or_up_ = true;              // 区别登录还是注册
    bool connected_ = false;            // 是否已经连接到服务器

    bool is_sign_{ false };             // 是否登录或注册成功
    bool is_save_info_{ false };        // 是否保存了客户端发来的用户信息

    QString user_;                      // 用户名
    QByteArray user_byte_;              // 用户名utf-8编码，用于网络传输
    QString pwd_;                       // 密码
    QByteArray pwd_byte_;               // 密码utf-8编码，用于网络传输
    UserInfo* info_ = nullptr;          // 保存服务器发送回来的用户信息

// ui
private:
    QLabel* user_lb_ = nullptr;
    QLabel* pwd_lb_ = nullptr;
    QLineEdit* user_le_ = nullptr;
    QLineEdit* pwd_le_ = nullptr;
    QPushButton* signin_pb_ = nullptr;
    QPushButton* signup_pb_ = nullptr;
};
