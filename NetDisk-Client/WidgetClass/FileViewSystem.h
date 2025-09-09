#ifndef FILEVIEWSYSTEM_H
#define FILEVIEWSYSTEM_H

#include "DisallowCopyAndMove.h"
#include "protocol.h"
#include <QAction>
#include <QWidget>
#include <QStackedWidget>
#include <QToolBar>
#include <QVBoxLayout>

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!! 已弃用 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

class FileViewSystem : public QWidget {
    Q_OBJECT

public:
    // 禁止拷贝或移动
    DISALLOW_COPY_AND_MOVE(FileViewSystem);

    explicit FileViewSystem(QWidget *parent = nullptr);
    ~FileViewSystem() override;

public:
    // 根据给定文件初始化文件视图系统
    void initView(std::vector<FileInfo> &vet);
    // 获取当前目录ID
    std::uint64_t getCurDirId();
    // 往文件视图中添加一个新文件项
    void addNewFileItem(TranPdu new_item, std::uint64_t file_id);
    //往文件视图添加一个新文件夹项
    void addNewDirItem(std::uint64_t new_id, std::uint64_t parent_id, QString new_dir_name);

private:
    void initUI();
    void initSignals();
    void initActions();
    void initToolBar();

    void initIcon(QListWidgetItem* item, QString file_type);
    void initChildListWidget(QListWidget* list);
    std::uint64_t initDirByteSize(QListWidgetItem* item);   // 递归更新文件夹空间信息

private slots:
    // 鼠标操作槽函数
    void handleItemClicked(QListWidgetItem* item);          // 项目被单击
    void handleItemDoubleClicked(QListWidgetItem* item);    // 项目被双击
    void handleCreateMenu(const QPoint& pos);               // 创建鼠标右键菜单
    void handleDisplayItemData(QListWidgetItem* item);      // 鼠标滑进后显示项目信息

    // action槽函数
    void handleUploadActTriggered();        // 上传按钮按下
    void handleDownloadActTriggered();      // 下载按钮按下
    void handleDeleteFileActTriggered();    // 删除按钮按下
    void handleBackActTriggered();          // 返回行为被按下
    void handleRefreshActTriggered();       // 刷新按键按下
    void handleCreateDirActTriggered();     // 创建文件夹按钮

signals:
    void refreshView();             // 刷新视图信号
    void download(ItemDate data);   // 下载信号
    void upload();                  // 上传信号
    void createDir(std::uint64_t parent_id,QString new_dir_name);   // 创建文件夹信号
    void deleteFile(std::uint64_t file_id, QString file_name);      // 删除文件夹信号

private:    // 成员变量
    QListWidgetItem *cur_item_{ nullptr };          // 当前项
    QListWidgetItem *cur_dir_item_{ nullptr };      // 当前文件夹项

    QListWidget *root_widget_{ nullptr };           // 根文件夹界面指针
    QMap<std::uint64_t, QListWidgetItem*> dir_map_; // 文件夹项ID对应文件夹项指针映射

private:    // 固定（不变）ui
    // 动作对象
    QAction *upload_act_{ nullptr };        // 上传文件
    QAction *download_act_{ nullptr };      // 下载文件
    QAction *delete_file_act_{ nullptr };   // 删除文件
    QAction *back_act_{ nullptr };          // 返回
    QAction *refresh_act_{ nullptr };       // 刷新视图
    QAction *create_dir_act_{ nullptr };    // 创建文件夹

    // 主要控件
    QToolBar *tool_tb_{ nullptr };              // 工具栏
    QStackedWidget *file_view_sw_{ nullptr };   // 用于显示不同目录文件
};

#endif // FILEVIEWSYSTEM_H
