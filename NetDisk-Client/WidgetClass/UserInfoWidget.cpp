#include "UserInfoWidget.h"
#include <QSpacerItem>

UserInfoWidget::UserInfoWidget(UserInfo* info,QWidget *parent)
    : QDialog(parent)
{
    initUI();

    user_le_->setText(QString(info->user));
    if (QString(info->is_vip) == "1") {
        is_vip_le_->setText("是");
    }
    else {
        is_vip_le_->setText("否");
    }
    capacity_sum_le_->setText(formatFileSize(QString::fromUtf8(info->capacity_sum).toULongLong()));
    capacity_used_le_->setText(formatFileSize(QString::fromUtf8(info->used_capacity).toULongLong()));
    vip_expire_le_->setText(QString(info->vip_date));

}

void UserInfoWidget::initUI() {
    // 初始化控件
    QFont lb_ft;
    lb_ft.setPointSize(14);
    QFont le_ft;
    le_ft.setPointSize(14);
    QFont pb_ft;
    pb_ft.setPointSize(14);

    user_lb_ =          new QLabel("用户名:",this);
    user_le_ =          new QLineEdit(this);
    is_vip_lb_ =        new QLabel("VIP:", this);
    is_vip_le_ =        new QLineEdit(this);
    capacity_sum_lb_ =  new QLabel("总空间:", this);
    capacity_sum_le_ =  new QLineEdit(this);
    capacity_used_lb_ = new QLabel("已使用空间:", this);
    capacity_used_le_ = new QLineEdit(this);
    vip_expire_lb_ =    new QLabel("VIP到期时间:", this);
    vip_expire_le_ =    new QLineEdit(this);
    ok_pb_ =            new QPushButton("确定", this);

    // 设置控件属性
    // 设置字体
    user_lb_->setFont(lb_ft);
    user_le_->setFont(le_ft);
    is_vip_lb_->setFont(lb_ft);
    is_vip_le_->setFont(le_ft);
    capacity_sum_lb_->setFont(lb_ft);
    capacity_sum_le_->setFont(le_ft);
    capacity_used_lb_->setFont(lb_ft);
    capacity_used_le_->setFont(le_ft);
    vip_expire_lb_->setFont(lb_ft);
    vip_expire_le_->setFont(le_ft);
    ok_pb_->setFont(pb_ft);
    // 禁用
    user_le_->setEnabled(false);
    is_vip_le_->setEnabled(false);
    capacity_sum_le_->setEnabled(false);
    capacity_used_le_->setEnabled(false);
    vip_expire_le_->setEnabled(false);

    // 布局
    // Label和LineEdit，弹簧和按钮合成水平布局
    QHBoxLayout* user_hbl = new QHBoxLayout;
    user_hbl->addWidget(user_lb_);
    user_hbl->addWidget(user_le_);
    QHBoxLayout* is_vip_hbl = new QHBoxLayout;
    is_vip_hbl->addWidget(is_vip_lb_);
    is_vip_hbl->addWidget(is_vip_le_);
    QHBoxLayout* capacity_sum_hbl = new QHBoxLayout;
    capacity_sum_hbl->addWidget(capacity_sum_lb_);
    capacity_sum_hbl->addWidget(capacity_sum_le_);
    QHBoxLayout* capacity_used_hbl = new QHBoxLayout;
    capacity_used_hbl->addWidget(capacity_used_lb_);
    capacity_used_hbl->addWidget(capacity_used_le_);
    QHBoxLayout* vip_expire_hbl = new QHBoxLayout;
    vip_expire_hbl->addWidget(vip_expire_lb_);
    vip_expire_hbl->addWidget(vip_expire_le_);
    QHBoxLayout* ok_hbl = new QHBoxLayout;
    ok_hbl->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    ok_hbl->addWidget(ok_pb_);

    // 所有水平布局合成垂直布局
    QVBoxLayout* main_vbl = new QVBoxLayout;
    main_vbl->addLayout(user_hbl);
    main_vbl->addLayout(is_vip_hbl);
    main_vbl->addLayout(capacity_sum_hbl);
    main_vbl->addLayout(capacity_used_hbl);
    main_vbl->addLayout(vip_expire_hbl);
    main_vbl->addLayout(ok_hbl);

    // 应用布局
    setLayout(main_vbl);
}
