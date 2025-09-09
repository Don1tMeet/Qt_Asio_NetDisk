#include "FileViewSystem.h"
#include <QDateTime>
#include <QInputDialog>
#include <QMenu>

FileViewSystem::FileViewSystem(QWidget *parent)
    : QWidget{parent}
{
    initUI();
    initSignals();
}

FileViewSystem::~FileViewSystem() {

}

void FileViewSystem::initView(std::vector<FileInfo> &vet) {
    std::uint64_t pre_id = 0;    // 用于界面刷新，恢复上次的选项
    if (cur_dir_item_ != nullptr) {
        pre_id = cur_dir_item_->data(Qt::UserRole).value<ItemDate>().id;	//暂存该ID
    }

    QListWidget *new_root_widegt = new QListWidget(this); // 创建新根目录
    initChildListWidget(new_root_widegt);                 // 初始化根目录

    if (vet.size() == 0) {  // 如果用户文件列表为空，设置并显示新根目录，直接返回
        // !!!!!!!!!!!!!!!!!! 疑问：这种情况时，为什么不删除旧数据（root_widget_的数据） !!!!!!!!!!!!!!!!!!!!!!
        root_widget_ = new_root_widegt;
        file_view_sw_->setCurrentWidget(root_widget_);  // 显示新根目录
        return;
    }

    dir_map_.clear();    // 清除旧数据
    for (const FileInfo& info : vet) {
        // !!!!!!!!!!!!!!!!!! 代码中有使用info，实际上data保存了info的数据，为了确保一致性，稍后改用data !!!!!!!!!!!!!!!!!!!!!!!!!!
        ItemDate data;  // 保存文件信息为文件视图中的项数据
        data.id = info.file_id;
        data.file_name = info.file_name;
        data.file_type = info.file_type;
        data.grade = info.dir_grade;
        data.parent_id = info.parent_dir;
        data.file_size = info.file_size;
        data.file_date = info.file_date;

        QListWidgetItem* item = new QListWidgetItem;	// 保存data的Item

        ItemDate parent;
        if (data.grade == 0) {  // 如果是根节点的项目，将其挂在根目录视图下
            new_root_widegt->addItem(item);
            data.parent = new_root_widegt;  // 设置父文件夹视图（QListWidget*）

            if (data.file_type == "d") {    // 如果是目录，需要创建子目录指针
                data.child = new QListWidget(new_root_widegt);  // 挂在根目录下
            }
        }
        else {  // 如果不是根节点，拿到它的父节点的ItemData
            parent = dir_map_.value(info.parent_dir)->data(Qt::UserRole).value<ItemDate>();
            if (parent.child) { // 父文件夹已经有文件视图（QListWidget*)
                parent.child->addItem(item);    // 挂在父文件夹视图之下
                data.parent = parent.child;     // 指向父文件夹视图
            }

            if (data.file_type == "d") {    // 如果是目录，需要创建子目录指针
                data.child = new QListWidget(parent.child);     // 挂在父目录下
            }
        }

        // 保存文件项数据
        QVariant va = QVariant::fromValue(data);
        item->setData(Qt::UserRole, va);    // 设置数据
        initIcon(item, data.file_type);     // 设置图标
        item->setText(data.file_name);      // 设置文本

        if (data.file_type == "d") {    // 如果是文件夹，则需要而外处理该文件夹视图
            initChildListWidget(data.child);    // 初始化子QListWidget
            dir_map_.insert(info.file_id, item);// 添加进dir_map_
        }
    }

    // 删除旧数据
    if (root_widget_) {
        root_widget_->clear();
        delete root_widget_;    // 删除该对象以及子对象。之所以不使用deleteLater()怕刷新频繁，造成内存积累
        root_widget_ = nullptr;
    }

    root_widget_ = new_root_widegt; // 设置新根目录

    // 如果之前的文件夹项目还在，重新指向该界面，否者为空
    QListWidgetItem *pre_dir_item = dir_map_.value(pre_id, nullptr);
    if (pre_dir_item != nullptr) {    // 之前的界面存在，设置为当前窗口
        // 获取当前文件夹项的数据（为了获取当前文件夹文件视图的指针）
        ItemDate cur = pre_dir_item->data(Qt::UserRole).value<ItemDate>();
        file_view_sw_->setCurrentWidget(cur.child); // 设置为当前窗体
        cur_dir_item_ = pre_dir_item;
    }
    else {  // 之前的界面不存在，设置根目录为当前窗口
        file_view_sw_->setCurrentWidget(root_widget_);
        cur_dir_item_ = nullptr;
    }

    // 更新文件夹空间信息，文件过多，可能会影响性能
    for (int i = 0; i < root_widget_->count(); ++i) {
        initDirByteSize(root_widget_->item(i));
    }
}

// 返回当前目录 id
uint64_t FileViewSystem::getCurDirId() {
    if (cur_dir_item_ == nullptr) {    //如果是根目录
        return 0;
    }

    ItemDate data = cur_dir_item_->data(Qt::UserRole).value<ItemDate>();
    return data.id;
}

// 往文件视图中添加一个新文件项
void FileViewSystem::addNewFileItem(TranPdu new_item, uint64_t file_id) {
    // 获取文件数据
    ItemDate data;
    data.id = file_id;  // user保存着服务端返回的文件ID，转换为数字
    data.file_name = new_item.file_name;
    data.file_type = QString(new_item.file_name).section('.', -1, -1);
    data.parent_id = new_item.parent_dir_id;
    data.file_size = new_item.file_size;
    QDateTime cur_time = QDateTime::currentDateTime();  // 获取当前系统时间
    data.file_date = cur_time.toString("yyyy-MM-dd hh:mm:ss");

    // 创建并设置文件项
    QListWidgetItem* item = new QListWidgetItem;
    initIcon(item, data.file_type);
    item->setText(data.file_name);
    // 添加文件项到对应视图
    QListWidgetItem* parent = dir_map_.value(data.parent_id, nullptr);
    if (parent == nullptr) {    // 不存在父节点，挂在根节点上
        root_widget_->addItem(item);

        data.parent = root_widget_;
        data.grade = 0;         // 级别为0
    }
    else {  // 存在父节点，挂在父节点上
        ItemDate parent_data = parent->data(Qt::UserRole).value<ItemDate>();
        parent_data.child->addItem(item);

        data.parent = parent_data.child;
        data.grade = parent_data.grade + 1; // 父级别+1
    }
    QVariant va = QVariant::fromValue(data);    // 设置数据
    item->setData(Qt::UserRole, va);

    // 向上递归更新文件夹空间
    while (parent) {    // 只要父节点不为空，继续向上
        ItemDate parent_data = parent->data(Qt::UserRole).value<ItemDate>();
        parent_data.file_size += data.file_size;
        parent->setData(Qt::UserRole, QVariant::fromValue(parent_data));    // 更新父节点数据
        parent = dir_map_.value(parent_data.parent_id, nullptr);    // 向上递归
    }
}

// 往文件视图添加一个新文件夹项
void FileViewSystem::addNewDirItem(uint64_t new_id, uint64_t parent_id, QString new_dir_name) {
    // 设置文件夹项数据
    ItemDate data;
    data.id = new_id;    // user保存着服务端返回的文件ID
    data.file_name = new_dir_name;
    data.file_type = "d";
    data.parent_id = parent_id;
    data.file_size = 0;
    QDateTime cur_time = QDateTime::currentDateTime();  // 获取当前系统时间
    data.file_date = cur_time.toString("yyyy-MM-dd hh:mm:ss");
    // 创建并设置文件夹项
    QListWidgetItem* item = new QListWidgetItem();
    initIcon(item, data.file_type);
    item->setText(data.file_name);

    // 添加文件夹项到对应视图
    QListWidgetItem* parent = dir_map_.value(data.parent_id, nullptr);
    if (parent == nullptr) {    // 不存在父节点，挂在根节点上
        root_widget_->addItem(item);

        data.parent = root_widget_;     // 设置父目录
        data.child = new QListWidget(root_widget_); // 创建该文件夹的文件视图
        data.grade = 0;         // 级别为0
    }
    else {  // 存在父节点，则挂在父结点上
        ItemDate parent_data = parent->data(Qt::UserRole).value<ItemDate>();
        parent_data.child->addItem(item);

        data.parent = parent_data.child;    // 设置父目录
        data.child = new QListWidget(parent_data.child); // 创建该文件夹的文件视图
        data.grade = parent_data.grade + 1; // 父级别+1
    }
    QVariant va = QVariant::fromValue(data);    //设置数据
    item->setData(Qt::UserRole, va);

    dir_map_.insert(data.id, item); // 添加到dir_map_

    // !!!!!!!!!!!!!!!!!!!!!!! 疑问：添加新文件夹不把文件夹id加入到dir_map_中吗 !!!!!!!!!!!!!!!!!!!
    // 这里在不影响源程序的情况下，将新文件夹加入到dir_map_
    if (!dir_map_.contains(data.id)) {
        dir_map_.insert(data.id, item);
    }
    // 初始化该文件夹项的视图
    initChildListWidget(data.child);
}

// 初始化UI
void FileViewSystem::initUI() {
    // 初始化控件
    initActions();    // 创建并设置 action
    initToolBar();    // 创建并设置 tool_tb_
    file_view_sw_ = new QStackedWidget(this);

    // 布局
    // 工具栏，文件视图合成垂直布局
    auto main_layout = new QVBoxLayout;
    main_layout->setSpacing(6);     // 设置布局内控件的间距
    main_layout->setContentsMargins(11, 11, 11, 11);   // 设置布局与父窗口的边距
    main_layout->addWidget(tool_tb_);
    main_layout->addWidget(file_view_sw_);

    // 应用布局
    setLayout(main_layout);
}

// 连接固定（不变）控件的信号与槽函数
void FileViewSystem::initSignals() {

    // action
    connect(upload_act_, &QAction::triggered,
            this, &FileViewSystem::handleUploadActTriggered);
    connect(download_act_, &QAction::triggered,
            this, &FileViewSystem::handleDownloadActTriggered);
    connect(delete_file_act_, &QAction::triggered,
            this, &FileViewSystem::handleDeleteFileActTriggered);
    connect(back_act_, &QAction::triggered,
            this, &FileViewSystem::handleBackActTriggered);
    connect(refresh_act_, &QAction::triggered,
            this, &FileViewSystem::handleRefreshActTriggered);
    connect(create_dir_act_, &QAction::triggered,
            this, &FileViewSystem::handleCreateDirActTriggered);
}

// 创建并设置 action
void FileViewSystem::initActions() {
    // 上传文件动作
    upload_act_ = new QAction(this);
    upload_act_->setIcon(QIcon(":/icon/icon/TranOpen.svg"));
    upload_act_->setText("上传文件");
    upload_act_->setMenuRole(QAction::NoRole);  // 跨平台兼容性，确保自定义动作在 macOS 系统中不会被系统菜单自动收纳

    // 下载文件动作
    download_act_ = new QAction(this);
    download_act_->setIcon(QIcon(":/icon/icon/downIcon.svg"));
    download_act_->setText("下载文件");
    download_act_->setMenuRole(QAction::NoRole);

    // 删除文件动作
    delete_file_act_ = new QAction(this);
    delete_file_act_->setIcon(QIcon(":/icon/icon/deleteIcon.svg"));
    delete_file_act_->setText("删除文件");
    delete_file_act_->setMenuRole(QAction::NoRole);

    // 返回动作
    back_act_ = new QAction(this);
    back_act_->setIcon(QIcon(":/icon/icon/bcakIcon.svg"));
    back_act_->setText("返回");
    back_act_->setMenuRole(QAction::NoRole);

    // 刷新视图动作
    refresh_act_ = new QAction(this);
    refresh_act_->setIcon(QIcon(":/icon/icon/refreshIcon.svg"));
    refresh_act_->setText("刷新视图");
    refresh_act_->setMenuRole(QAction::NoRole);

    // 创建文件夹动作
    create_dir_act_ = new QAction(this);
    create_dir_act_->setIcon(QIcon(":/icon/icon/createDir.svg"));
    create_dir_act_->setText("创建文件夹");
    create_dir_act_->setMenuRole(QAction::NoRole);

}

// 创建并设置 tool_tb_
void FileViewSystem::initToolBar() {
    // 创建工具栏
    tool_tb_ = new QToolBar("文件操作", this);
    // 设置显示方式，同时显示图像和文本，文本在图像旁边
    tool_tb_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // 添加动作到工具栏
    tool_tb_->addAction(upload_act_);
    tool_tb_->addAction(download_act_);
    tool_tb_->addAction(delete_file_act_);
    tool_tb_->addSeparator();   // 添加分割线
    tool_tb_->addAction(back_act_);
    tool_tb_->addAction(refresh_act_);
    tool_tb_->addSeparator();
    tool_tb_->addAction(create_dir_act_);
}

// 设置图标
void FileViewSystem::initIcon(QListWidgetItem *item, QString file_type) {
    // !!!!!!!!!!!!!!!!!!!!!!!!!!! 后续可将文件类型改为枚举类型 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (file_type == "d") {
        item->setIcon(QIcon(":/icon/icon/dirIcon.svg"));
    }
    else if (file_type == "jpg" || file_type == "png") {
        item->setIcon(QIcon(":/icon/icon/pitureIcon.svg"));
    }
    else if (file_type == "mp4") {
        item->setIcon(QIcon(":/icon/icon/MP4Icon.svg"));
    }
    else if (file_type == "txt") {
        item->setIcon(QIcon(":/icon/icon/txtIcon.svg"));
    }
    else if (file_type == "mp3") {
        item->setIcon(QIcon(":/icon/icon/mp3Icon.svg"));
    }
    else {
        item->setIcon(QIcon(":/icon/icon/otherIcon.svg"));
    }
}

// 初始化每个QlistWidget属性，和连接信号
void FileViewSystem::initChildListWidget(QListWidget *list) {
    if (list == nullptr) {
        return;
    }

    list->setViewMode(QListView::IconMode); // 显示模式为图标模式
    list->setFlow(QListView::LeftToRight);  // 图标排列为从在到右,从上到下
    list->setIconSize(QSize(64, 64));       // 图标大小
    list->setGridSize(QSize(120, 100));     // 网格大小(控制项的间距和排列)
    list->setSpacing(10);                   // 设置表项之间的距离

    list->setResizeMode(QListView::Adjust);             // 随父窗体变大变小
    list->setContextMenuPolicy(Qt::CustomContextMenu);	// 设置自定义内容菜单
    list->setMouseTracking(true);						// 设置鼠标跟踪
    list->setDragEnabled(false);                        // 禁用列表项拖拽

    file_view_sw_->addWidget(list); // 将其添加到文件视图系统（堆栈布局窗体）

    // 连接信号和槽函数
    connect(list, &QListWidget::itemClicked,
            this, &FileViewSystem::handleItemClicked);
    connect(list, &QListWidget::itemDoubleClicked,
            this, &FileViewSystem::handleItemDoubleClicked);
    connect(list, &QListWidget::customContextMenuRequested,
            this, &FileViewSystem::handleCreateMenu);
    connect(list, &QListWidget::itemEntered,
            this, &FileViewSystem::handleDisplayItemData);
}

// 递归更新文件夹空间信息
std::uint64_t FileViewSystem::initDirByteSize(QListWidgetItem *item) {
    if (!item) {
        return 0;
    }

    // 获取当前项目的数据
    ItemDate data = item->data(Qt::UserRole).value<ItemDate>();
    std::uint64_t total_size = 0;  // 文件大小

    // 如果当前项目是一个文件夹，递归计算其子项的大小，不是文件夹，则为文件大小
    if (data.child) {   // 只有是文件夹且已经创建了该文件夹视图的时候，data.child才不为空
        total_size = 0; // 文件夹本身大小为 0
        for (int i = 0; i < data.child->count(); ++i) {
            QListWidgetItem* child_item = data.child->item(i);
            total_size += initDirByteSize(child_item);
        }
    }
    else {  // 不为文件夹
        total_size = data.file_size;    // 总大小为文件大小
    }

    // 更新当前项的大小并保存
    data.file_size = total_size;
    item->setData(Qt::UserRole, QVariant::fromValue(data));

    return total_size;
}

// 处理文件项点击信号的槽函数
void FileViewSystem::handleItemClicked(QListWidgetItem *item) {
    if (item == nullptr) {
        cur_item_ = nullptr;
        return;
    }

    ItemDate data = item->data(Qt::UserRole).value<ItemDate>();

    if (data.file_type == "d") {    // 如果是文件夹
        download_act_->setEnabled(false);   // 文件夹不能下载，禁用
    }
    else {
        download_act_->setEnabled(true);    // 其它文件可下载
    }
    cur_item_ = item;   // 改变当前选中的item
}

// 处理文件项双击信号的槽函数
void FileViewSystem::handleItemDoubleClicked(QListWidgetItem *item) {
    if (item == nullptr) {
        cur_item_ = nullptr;
        return;
    }

    ItemDate data = item->data(Qt::UserRole).value<ItemDate>();

    if (data.file_type == "d") {    //如果是文件夹
        cur_dir_item_ = item;
        file_view_sw_->setCurrentWidget(data.child);    // 切换当前显示的视图
        download_act_->setEnabled(false);   // 禁用下载
        cur_item_ = nullptr;    // 进入新目录时，清除所有文件项
    }
    else {
        // 其它文件双击不做额外处理
        cur_item_ = item;
    }
}

// 处理右键信号的槽函数
void FileViewSystem::handleCreateMenu(const QPoint &pos) {
    // 获取发送信号的对象（即点击了右键的文件视图）
    QListWidget* list = static_cast<QListWidget*>(sender());
    if (list == nullptr) {
        cur_item_ = nullptr;
        return;
    }

    // 创建菜单
    // !!!!!!!!!!!!!!!!!!!!!!!!!! 后续可把menu改为成员，将不可以触发的action设置为setEnable(false) !!!!!!!!!!!!!!!!!!!!!!!!
    QMenu menu(this);
    ItemDate data;
    QListWidgetItem *item = list->itemAt(pos);  // 获取pos处的item
    if (item != nullptr) {  // pos位于文件项上
        data = item->data(Qt::UserRole).value<ItemDate>();

        if (data.file_type == "d") {    // 文件夹
            menu.addAction(delete_file_act_);
        }
        else {  // 其它文件
            menu.addAction(delete_file_act_);
            menu.addAction(download_act_);
        }

        cur_item_ = item;
    }
    else {  // 空白处
        menu.addAction(back_act_);
        menu.addAction(refresh_act_);
        menu.addAction(upload_act_);
        menu.addAction(create_dir_act_);

        cur_item_ = nullptr;
    }

    // 在当前位置显示菜单
    menu.exec(QCursor::pos());
}

// 处理鼠标进入文件项信号的槽函数
void FileViewSystem::handleDisplayItemData(QListWidgetItem *item) {
    if (item == nullptr) {
        return;
    }
    // 获取文件项数据
    ItemDate data = item->data(Qt::UserRole).value<ItemDate>();
    if (data.file_type == "d") {
        data.file_type = "文件夹";
    }

    // 格式化信息
    QString info = QString("文件名称:  %1\n文件类型:  %2\n文件大小:  %3 bytes\n创建日期:  %4")
                       .arg(data.file_name, data.file_date, formatFileSize(data.file_size), data.file_date);

    // 设置文件项悬浮数据
    item->setToolTip(info);
}

// 处理上传动作触发的槽函数
void FileViewSystem::handleUploadActTriggered() {
    emit upload();  // 发送上传信号

    cur_item_ = nullptr;    // 清除选择文件
}

// 处理下载动作触发的槽函数
void FileViewSystem::handleDownloadActTriggered() {
    if (cur_item_ == nullptr) {
        return;
    }
    // 获取当前选中文件项的数据
    ItemDate data = cur_item_->data(Qt::UserRole).value<ItemDate>();
    if (data.file_type != "d") {    // 只能下载普通文件
        emit download(data);    // 发送下载信号
    }

    cur_item_ = nullptr;    // 清除选择文件
}

// 处理删除动作触发的槽函数
void FileViewSystem::handleDeleteFileActTriggered() {
    if (cur_item_ == nullptr) {
        return;
    }
    // 获取要删除文件的 id 和文件名（主要是后缀名）
    ItemDate data = cur_item_->data(Qt::UserRole).value<ItemDate>();

    std::uint64_t file_id = data.id;
    QString file_name = data.file_name;
    if (data.file_type == "d") {
        file_name = "dir.d";    //服务端删除文件不需要文件名，只需要ID和后缀名。所以只需要传递后缀名可以
    }
    else {
        file_name = "file.other";
    }
    emit deleteFile(file_id, file_name);  // 发送删除信号

    cur_item_ = nullptr;    // 清除选择的文件
}

// 处理返回动作触发的槽函数
void FileViewSystem::handleBackActTriggered() {
    if (cur_dir_item_ == nullptr) { // 当前目录为根目录时，cur_dir_item会等于空
        return;
    }

    // 获取要返回目录的数据
    ItemDate data = cur_dir_item_->data(Qt::UserRole).value<ItemDate>();

    if(data.parent_id == 0) {   // 如果是根目录下的目录
        file_view_sw_->setCurrentWidget(root_widget_);  // 显示根目录
    }
    else {  // 不是根目录下的目录
        file_view_sw_->setCurrentWidget(data.parent);   // 显示父前目录
    }
    // 修改当前文件夹
    cur_dir_item_ = dir_map_.value(data.parent_id, nullptr);

    cur_item_ = nullptr;    // 更换目录后，清除选择的文件
}

// 处理刷新动作触发的槽函数
void FileViewSystem::handleRefreshActTriggered() {
    emit refreshView();	// 发送刷新信号
}

// 处理创建文件夹动作触发的槽函数
void FileViewSystem::handleCreateDirActTriggered() {
    QString new_dir_name = QInputDialog::getText(this, "创建文件夹", "文件夹名：");
    if (new_dir_name.isEmpty()) {
        return;
    }
    std::uint64_t parent_id = 0;
    if (cur_dir_item_ == nullptr) { // 当前为根目录
        parent_id = 0;
    }
    else {  // 不为根目录，创建在当前目录下
        ItemDate data = cur_dir_item_->data(Qt::UserRole).value<ItemDate>();
        parent_id = data.id;
    }

    emit createDir(parent_id, new_dir_name);  // 发送创建文件夹信号
}
