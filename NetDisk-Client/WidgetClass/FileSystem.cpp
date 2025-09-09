#include "FileSystem.h"
#include <QDateTime>


FileSystem::FileSystem(QWidget *parent)
    : QWidget{parent}
{
    initUI();
    initModel();
    initSignals();
}

FileSystem::~FileSystem() {
    clearAllItem();
}

void FileSystem::initView(std::vector<FileInfo> &vet) {
    clearAllItem();
    for (int i=0; i<vet.size(); ++i) {
        auto& info = vet[i];
        ItemDate data;  // 保存文件信息为文件视图中的项数据
        data.id = info.file_id;
        data.file_name = info.file_name;
        data.file_type = info.file_type;
        data.grade = info.dir_grade;
        data.parent_id = info.parent_dir;
        data.file_size = info.file_size;
        data.file_date = info.file_date;

        // 创建文件项
        QStandardItem* item = new QStandardItem;
        QVariant va = QVariant::fromValue(data);
        item->setData(va, Qt::UserRole);    // 设置数据
        initIcon(item, data.file_type);     // 设置图标
        item->setText(data.file_name);      // 设置文本
        // 设置文件项悬浮数据
        QString tip_info = QString("文件名称:  %1\n文件类型:  %2\n文件大小:  %3 bytes\n创建日期:  %4")
                               .arg(data.file_name, data.file_date, formatFileSize(data.file_size), data.file_date);
        item->setToolTip(tip_info);

        if (data.file_type == "d") {    // 如果是文件夹，加入到映射中
            dir_map_.insert(data.id, item);
        }
        // 加入到对应文件夹
        dir_files[data.parent_id].append(item);
    }
    // 显示目录
    if (dir_map_.contains(cur_dir_id)) {
        cur_dir_item_ = dir_map_[cur_dir_id];
        loadDir(cur_dir_id);
    }
    else {  // 之前的目录已经被删除，返回根目录
        cur_dir_id = 0;
        cur_dir_item_ = nullptr;
        loadDir(0);
    }
    cur_item_ = nullptr;

    // 更新文件夹空间信息，文件过多，可能会影响性能
    for (int i = 0; i < dir_files[0].size(); ++i) {
        initDirByteSize(dir_files[0][i]);
    }
}

void FileSystem::clearAllItem() {
    QMutableMapIterator<std::uint64_t, QList<QStandardItem*>> it(dir_files);
    while (it.hasNext()) {
        it.next(); // 移动到下一个键值对
        QList<QStandardItem*>& itemList = it.value();
        // 批量删除列表中的所有 QStandardItem 对象
        qDeleteAll(itemList); // 对每个 item 调用 delete，释放内存
        // 清空列表
        itemList.clear();
    }

    dir_map_.clear();
    dir_files.clear();
}

void FileSystem::loadDir(uint64_t dir_id) {
    // file_model_->clear();
    // 取出所有item，不删除
    while (file_model_->rowCount() > 0) {
        file_model_->takeRow(0);
    }
    // 显示当前目录
    cur_dir_id = dir_id;
    cur_dir_item_ = dir_map_.value(dir_id, nullptr);

    for (int i=0; i!=dir_files[dir_id].size(); ++i) {
        file_model_->appendRow(dir_files[dir_id][i]);
    }

    proxy_model_->sort(0);
}

// 返回当前目录 id
uint64_t FileSystem::getCurDirId() {
    return cur_dir_id;
}

// 往文件视图中添加一个新文件项
void FileSystem::addNewFileItem(TranPdu new_item, uint64_t file_id) {
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
    QStandardItem* item = new QStandardItem;
    initIcon(item, data.file_type);
    item->setText(data.file_name);
    QString tip_info = QString("文件名称:  %1\n文件类型:  %2\n文件大小:  %3 bytes\n创建日期:  %4")
                           .arg(data.file_name, data.file_date, formatFileSize(data.file_size), data.file_date);
    item->setToolTip(tip_info);

    // 添加文件项到对应视图
    dir_files[data.parent_id].append(item);
    file_model_->appendRow(item);

    QStandardItem* parent = dir_map_.value(data.parent_id, nullptr);
    if (parent == nullptr) {    // 不存在父节点，挂在根节点上
        data.grade = 0;         // 级别为0
    }
    else {  // 存在父节点，挂在父节点上
        ItemDate parent_data = parent->data(Qt::UserRole).value<ItemDate>();
        data.grade = parent_data.grade + 1; // 父级别+1
    }
    QVariant va = QVariant::fromValue(data);    // 设置数据
    item->setData(va, Qt::UserRole);

    // 向上递归更新文件夹空间
    // !!!!!!!!!!!!!!!!!!!!!!!!!! 悬浮显示未更新 !!!!!!!!!!!!!!!!!!!!!!!!!!!
    while (parent) {    // 只要父节点不为空，继续向上
        ItemDate parent_data = parent->data(Qt::UserRole).value<ItemDate>();
        parent_data.file_size += data.file_size;
        parent->setData(QVariant::fromValue(parent_data), Qt::UserRole);    // 更新父节点数据
        parent = dir_map_.value(parent_data.parent_id, nullptr);    // 向上递归
    }
}

// 往文件视图添加一个新文件夹项
void FileSystem::addNewDirItem(uint64_t new_id, uint64_t parent_id, QString new_dir_name) {
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
    QStandardItem* item = new QStandardItem();
    initIcon(item, data.file_type);
    item->setText(data.file_name);
    QString tip_info = QString("文件名称:  %1\n文件类型:  %2\n文件大小:  %3 bytes\n创建日期:  %4")
                           .arg(data.file_name, data.file_date, formatFileSize(data.file_size), data.file_date);
    item->setToolTip(tip_info);

    // 添加文件夹项到对应视图
    dir_files[data.parent_id].append(item);
    file_model_->appendRow(item);

    QStandardItem* parent = dir_map_.value(data.parent_id, nullptr);
    if (parent == nullptr) {    // 不存在父节点，挂在根节点上
        data.grade = 0;         // 级别为0
    }
    else {  // 存在父节点，则挂在父结点上
        ItemDate parent_data = parent->data(Qt::UserRole).value<ItemDate>();
        data.grade = parent_data.grade + 1; // 父级别+1
    }
    QVariant va = QVariant::fromValue(data);    //设置数据
    item->setData(va, Qt::UserRole);

    // 将新文件夹加入到dir_map_
    if (!dir_map_.contains(data.id)) {
        dir_map_.insert(data.id, item);
    }
}

void FileSystem::initUI() {
    initActions();
    initToolBar();

    // 创建不同的视图
    list_view_ = new QListView(this);

    // 设置视图属性
    list_view_->setViewMode(QListView::IconMode);
    list_view_->setFlow(QListView::LeftToRight);  // 图标排列为从在到右,从上到下
    list_view_->setIconSize(QSize(64, 64));       // 图标大小
    list_view_->setGridSize(QSize(120, 100));     // 网格大小(控制项的间距和排列)
    list_view_->setSpacing(10);                   // 设置表项之间的距离

    list_view_->setResizeMode(QListView::Adjust);             // 随父窗体变大变小
    list_view_->setContextMenuPolicy(Qt::CustomContextMenu);	// 设置自定义内容菜单
    list_view_->setMouseTracking(true);						// 设置鼠标跟踪
    list_view_->setDragEnabled(false);                        // 禁用列表项拖拽

    // 创建视图切换栈
    view_stack_ = new QStackedWidget(this);
    view_stack_->addWidget(list_view_);

    // 布局
    auto *main_layout = new QVBoxLayout;
    main_layout->setSpacing(6);     // 设置布局内控件的间距
    main_layout->setContentsMargins(11, 11, 11, 11);   // 设置布局与父窗口的边距
    main_layout->addWidget(tool_tb_);
    main_layout->addWidget(view_stack_);

    setLayout(main_layout);
}

void FileSystem::initModel() {
    // 文件模型
    // file_model_ = new QStandardItemModel(0, 4, this);
    // file_model_->setHeaderData(0, Qt::Horizontal, tr("Name"));
    // file_model_->setHeaderData(1, Qt::Horizontal, tr("Size"));
    // file_model_->setHeaderData(2, Qt::Horizontal, tr("Modified"));
    // file_model_->setHeaderData(3, Qt::Horizontal, tr("Type"));

    file_model_ = new QStandardItemModel(this);
    proxy_model_ = new FileSortProxy;
    proxy_model_->setSourceModel(file_model_); // 关联源模型

    // 将模型设置到视图
    list_view_->setModel(proxy_model_); // 视图使用代理模型
}

// 连接固定（不变）控件的信号与槽函数
void FileSystem::initSignals() {

    // 连接信号和槽函数
    // 鼠标
    connect(list_view_, &QListView::clicked,
            this, &FileSystem::handleItemClicked);
    connect(list_view_, &QListView::doubleClicked,
            this, &FileSystem::handleItemDoubleClicked);
    connect(list_view_, &QListView::customContextMenuRequested,
            this, &FileSystem::handleCreateMenu);

    // action
    connect(upload_act_, &QAction::triggered,
            this, &FileSystem::handleUploadActTriggered);
    connect(download_act_, &QAction::triggered,
            this, &FileSystem::handleDownloadActTriggered);
    connect(delete_file_act_, &QAction::triggered,
            this, &FileSystem::handleDeleteFileActTriggered);
    connect(back_act_, &QAction::triggered,
            this, &FileSystem::handleBackActTriggered);
    connect(refresh_act_, &QAction::triggered,
            this, &FileSystem::handleRefreshActTriggered);
    connect(create_dir_act_, &QAction::triggered,
            this, &FileSystem::handleCreateDirActTriggered);
}

// 创建并设置 action
void FileSystem::initActions() {
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
void FileSystem::initToolBar() {
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
void FileSystem::initIcon(QStandardItem *item, QString file_type) {
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

uint64_t FileSystem::initDirByteSize(QStandardItem *item) {
    if (!item) {
        return 0;
    }

    // 获取当前项目的数据
    ItemDate data = item->data(Qt::UserRole).value<ItemDate>();
    std::uint64_t total_size = 0;  // 文件大小

    // 如果当前项目是一个文件夹，递归计算其子项的大小，不是文件夹，则为文件大小
    if (data.file_type == "d") {
        total_size = 0; // 文件夹本身大小为 0
        for (int i = 0; i < dir_files[data.id].size(); ++i) {
            QStandardItem* child_item = dir_files[data.id][i];
            total_size += initDirByteSize(child_item);
        }
    }
    else {  // 不为文件夹
        total_size = data.file_size;    // 总大小为文件大小
    }

    // 更新当前项的大小并保存
    data.file_size = total_size;
    item->setData(QVariant::fromValue(data), Qt::UserRole);

    return total_size;
}

// 处理文件项点击信号的槽函数
void FileSystem::handleItemClicked(const QModelIndex& proxy_index) {
    QModelIndex index = proxy_model_->mapToSource(proxy_index);
    // 检查索引是否有效（避免点击空白区域）
    if (!index.isValid()) {
        cur_item_ = nullptr;
        return;
    }

    // 通过索引获取对应的 QStandardItem
    QStandardItem *item = file_model_->itemFromIndex(index);
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
void FileSystem::handleItemDoubleClicked(const QModelIndex& proxy_index) {
    QModelIndex index = proxy_model_->mapToSource(proxy_index);
    // 检查索引是否有效（避免点击空白区域）
    if (!index.isValid()) {
        cur_item_ = nullptr;
        return;
    }

    // 通过索引获取对应的 QStandardItem
    QStandardItem *item = file_model_->itemFromIndex(index);
    if (item == nullptr) {
        cur_item_ = nullptr;
        return;
    }

    ItemDate data = item->data(Qt::UserRole).value<ItemDate>();
    if (data.file_type == "d") {    //如果是文件夹
        cur_dir_item_ = item;
        loadDir(data.id);   // 加载当前文件夹

        download_act_->setEnabled(false);   // 禁用下载
        cur_item_ = nullptr;    // 进入新目录时，清除所有文件项
    }
    else {
        // 其它文件双击不做额外处理
        cur_item_ = item;
    }
}

// 处理右键信号的槽函数
void FileSystem::handleCreateMenu(const QPoint &pos) {
    // 将鼠标位置转换为模型索引
    QModelIndex proxy_index = list_view_->indexAt(pos);
    QModelIndex index = proxy_model_->mapToSource(proxy_index);

    // !!!!!!!!!!!!!!!!!!!!!!!!!! 后续可把menu改为成员，将不可以触发的action设置为setEnable(false) !!!!!!!!!!!!!!!!!!!!!!!!
    QMenu menu(this);
    // 判断是否点击了有效项（非空白区域）
    if (!index.isValid()) { // 空白
        menu.addAction(back_act_);
        menu.addAction(refresh_act_);
        menu.addAction(upload_act_);
        menu.addAction(create_dir_act_);

        cur_item_ = nullptr;
        menu.exec(QCursor::pos());
        return;
    }

    // 通过模型获取 QStandardItem
    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(qobject_cast<FileSortProxy*>(list_view_->model())->sourceModel());
    if (!model) {
        cur_item_ = nullptr;
        return;
    }

    QStandardItem *item = model->itemFromIndex(index);
    if (!item) {
        cur_item_ = nullptr;
        return;
    }

    ItemDate data;
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

    // 在当前位置显示菜单
    menu.exec(QCursor::pos());
}

// 处理上传动作触发的槽函数
void FileSystem::handleUploadActTriggered() {
    emit upload();  // 发送上传信号

    cur_item_ = nullptr;    // 清除选择文件
}

// 处理下载动作触发的槽函数
void FileSystem::handleDownloadActTriggered() {
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
void FileSystem::handleDeleteFileActTriggered() {
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
void FileSystem::handleBackActTriggered() {
    // 根目录
    if (cur_dir_id == 0 || cur_dir_item_ == nullptr) {
        return;
    }

    // 获取要返回目录的数据
    ItemDate data = cur_dir_item_->data(Qt::UserRole).value<ItemDate>();

    loadDir(data.parent_id);

    cur_item_ = nullptr;    // 更换目录后，清除选择的文件
}

// 处理刷新动作触发的槽函数
void FileSystem::handleRefreshActTriggered() {
    emit refreshView();	// 发送刷新信号
}

// 处理创建文件夹动作触发的槽函数
void FileSystem::handleCreateDirActTriggered() {
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
