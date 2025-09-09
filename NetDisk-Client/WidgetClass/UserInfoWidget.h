#ifndef USERINFOWIDGET_H
#define USERINFOWIDGET_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "DisallowCopyAndMove.h"
#include "protocol.h"


class UserInfoWidget : public QDialog {
    Q_OBJECT

public:
    // 禁止拷贝和移动
    DISALLOW_COPY_AND_MOVE(UserInfoWidget);

    UserInfoWidget(UserInfo* info,QWidget *parent = nullptr);
    ~UserInfoWidget() = default;

private:
    void initUI();
// ui
private:
    QLabel *user_lb_{ nullptr };                // 用户名
    QLineEdit *user_le_{ nullptr };
    QLabel *is_vip_lb_{ nullptr };              // 是否是VIP
    QLineEdit *is_vip_le_{ nullptr };
    QLabel *capacity_sum_lb_{ nullptr };        // 总空间
    QLineEdit *capacity_sum_le_{ nullptr };
    QLabel *capacity_used_lb_{ nullptr };       // 使用空间
    QLineEdit *capacity_used_le_{ nullptr };
    QLabel *vip_expire_lb_{ nullptr };          // VIP到期时间
    QLineEdit *vip_expire_le_{ nullptr };
    QPushButton *ok_pb_{ nullptr };             // 确定按钮
};

#endif // USERINFOWIDGET_H
