#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "protocol.h"
#include <QObject>
#include <QAction>
#include <QMenu>
#include <QWidget>
#include <QToolBar>
#include <QListView>
#include <QTreeView>
#include <QTableView>
#include <QStackedWidget>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QInputDialog>

// 代理模型，用于文件排序
class FileSortProxy : public QSortFilterProxyModel {
    Q_OBJECT
protected:
    // 重写比较方法
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
        // 获取源模型中的数据（假设按DisplayRole比较）
        ItemDate left_data = sourceModel()->data(left, Qt::UserRole).value<ItemDate>();
        ItemDate right_data = sourceModel()->data(right, Qt::UserRole).value<ItemDate>();

        // 文件夹优先
        if (left_data.file_type == "d" && right_data.file_type != "d") {
            return true;
        }
        if (left_data.file_type != "d" && right_data.file_type == "d") {
            return false;
        }
        // 按类型排序
        if (left_data.file_type != right_data.file_type) {
            return left_data.file_type < right_data.file_type;
        }
        // 按文件名排序
        return left_data.file_name < right_data.file_name;
    }
};


class FileSystem : public QWidget {
    Q_OBJECT

public:
    // 禁止拷贝或移动
    // DISALLOW_COPY_AND_MOVE(FileSystem);

    explicit FileSystem(QWidget *parent = nullptr);
    ~FileSystem() override;

public:
    // 根据给定文件初始化文件视图系统
    void initView(std::vector<FileInfo> &vet);
    void clearAllItem();
    void loadDir(std::uint64_t dir_id);
    // 获取当前目录ID
    std::uint64_t getCurDirId();
    // 往文件视图中添加一个新文件项
    void addNewFileItem(TranPdu new_item, std::uint64_t file_id);
    //往文件视图添加一个新文件夹项
    void addNewDirItem(std::uint64_t new_id, std::uint64_t parent_id, QString new_dir_name);

private:
    void initUI();
    void initModel();
    void initSignals();
    void initActions();
    void initToolBar();

    void initIcon(QStandardItem* item, QString file_type);
    std::uint64_t initDirByteSize(QStandardItem* item);   // 递归更新文件夹空间信息

private slots:
    // // 鼠标操作槽函数
    void handleItemClicked(const QModelIndex& index);           // 项目被单击
    void handleItemDoubleClicked(const QModelIndex& index);     // 项目被双击
    void handleCreateMenu(const QPoint& pos);                   // 创建鼠标右键菜单

    // // action槽函数
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
    void createDir(std::uint64_t parent_id, QString new_dir_name);   // 创建文件夹信号
    void deleteFile(std::uint64_t file_id, QString file_name);      // 删除文件夹信号


private:
    // 视图相关
    QStandardItem *cur_item_{ nullptr };          // 当前项
    QStandardItem *cur_dir_item_{ nullptr };      // 当前文件夹项
    std::uint64_t cur_dir_id{ 0 };      // 当前文件夹 id

    QMap<std::uint64_t, QStandardItem*> dir_map_; // 文件夹项ID对应文件夹项指针映射

    QMap<std::uint64_t, QList<QStandardItem*>> dir_files;   // 指定文件夹ID下的文件

    QListView *list_view_{ nullptr };
    QStackedWidget *view_stack_{ nullptr };
    QStandardItemModel *file_model_{ nullptr };
    FileSortProxy *proxy_model_{ nullptr };

    // 动作对象
    QAction *upload_act_{ nullptr };        // 上传文件
    QAction *download_act_{ nullptr };      // 下载文件
    QAction *delete_file_act_{ nullptr };   // 删除文件
    QAction *back_act_{ nullptr };          // 返回
    QAction *refresh_act_{ nullptr };       // 刷新视图
    QAction *create_dir_act_{ nullptr };    // 创建文件夹

    // 主要控件
    QToolBar *tool_tb_{ nullptr };              // 工具栏
};

#endif // FILESYSTEM_H
