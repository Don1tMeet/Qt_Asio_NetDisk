#include "ShortTaskManager.h"

ShortTaskManager::ShortTaskManager(std::shared_ptr<SR_Tool> sr_tool, std::shared_ptr<TaskQue> task_queue, QObject* parent)
    : QObject(parent)
    , file_cnt_(0)
    , file_vet_(std::make_shared<std::vector<FileInfo>>(0))
    , sr_tool_(sr_tool)
    , task_queue_(task_queue)
{
    initSignals();
}

ShortTaskManager::~ShortTaskManager() {
    // 再次确保线程池关闭
    task_queue_->close();
}

// 获取文件列表
void ShortTaskManager::getFileList() {
    {
        std::lock_guard<std::mutex> lock(file_mutex_);
        if (file_cnt_ != 0) {   // 不等于0说明，之前的操作没有执行完，因此拒绝执行
            emit error("getFileList(): can't execute");
            return;
        }
    }
    bool status{false};
    // 发送请求
    PDU pdu;
    pdu.header.type = ProtocolType::PDU_TYPE;
    pdu.header.body_len = PDU_BODY_BASE_LEN;
    pdu.code = Code::CD;
    // !!!!!!!!!!!!!!!!!!!! 可改为异步发送 !!!!!!!!!!!!!!!!!!!!!!!!!!
    status = sr_tool_->sendPDU(pdu);
    if (false == status) {
        emit error("getFileList(): send pdu error");
        return;
    }

    qDebug() << "send get file list request ok";
}

// 创建文件夹
void ShortTaskManager::makeDir(quint64 parent_id, QString new_dir_name) {
    // 发送请求
    PDU pdu;
    pdu.header.type = ProtocolType::PDU_TYPE;
    pdu.header.body_len = PDU_BODY_BASE_LEN;
    pdu.code = Code::MAKEDIR;

    // 确保文件名不会超过 pdu.filename 的大小
    QByteArray dir_name_bytes = new_dir_name.toUtf8();
    if (static_cast<unsigned long long>(dir_name_bytes.size()) >= sizeof(pdu.file_name)) {
        emit error("Directory name too long");
        return;
    }
    std::uint64_t pid = htonll(parent_id);  // 转换字节序
    pdu.msg_len = sizeof(pid);
    pdu.header.body_len = PDU_BODY_BASE_LEN + pdu.msg_len;
    memcpy(pdu.file_name, dir_name_bytes.data(), dir_name_bytes.size());    // 复制文件夹名到 pdu.file_name
    memcpy(pdu.msg, (char*)&pid, sizeof(pid));  // 复制父文件夹ID到 pdu.msg

    sr_tool_->sendPDU(pdu); // 发送

    qDebug() << "send make dir request ok";
}

// 删除文件
void ShortTaskManager::delFile(quint64 file_id, QString file_name) {
    PDU pdu;
    pdu.header.type = ProtocolType::PDU_TYPE;
    pdu.header.body_len = PDU_BODY_BASE_LEN;
    pdu.code = Code::DELETEFILE;
    pdu.msg_len = 0;

    QByteArray file_name_bytes = file_name.toUtf8();    // 转成utf-8编码
    if (static_cast<uint64_t>(file_name_bytes.size()) >= sizeof(pdu.file_name)) {
        emit error("delFile error: file_name too long");
        return;
    }
    // 这里在pdu的pwd字段保存file_id，注意在服务端区分
    file_id = htonll(file_id);  // 转换字节序
    memcpy(pdu.pwd, (char*)&file_id, sizeof(file_id));  // 复制文件ID到 pdu.pwd
    memcpy(pdu.file_name, file_name_bytes.data(), file_name_bytes.size());  // 复制文件名到 pdu.filename

    // 发送 PDU 请求
    sr_tool_->sendPDU(pdu);

    qDebug() << "send delete file request ok";
}

void ShortTaskManager::handleRecvPDURespond(std::shared_ptr<PDURespond> pdu) {
    // 接收PDURespond，并根据pdu->code交给其它线程处理
    // 这里多线程中使用this，因此需要确保线程执行中，this不会销毁
    // 因此使用需要传入this本身的智能指针
    qDebug() << "recv PDURespond ok, code:" << pdu->code;
    switch (pdu->code) {
        case Code::CD:          task_queue_->addTask(std::bind(&ShortTaskManager::handleCD, this, pdu)); break;
        case Code::MAKEDIR:     task_queue_->addTask(std::bind(&ShortTaskManager::handleMakeDir, this, pdu)); break;
        case Code::DELETEFILE:  task_queue_->addTask(std::bind(&ShortTaskManager::handleDeleteFile, this, pdu)); break;
    }
}

void ShortTaskManager::handleRecvFileInfo(std::shared_ptr<FileInfo> pdu) {
    task_queue_->addTask(std::bind(&ShortTaskManager::handleFileInfo, this, pdu));
}

void ShortTaskManager::handleCD(std::shared_ptr<PDURespond> pdu) {
    // 如果服务端回复错误信息
    qDebug() << "recv get file list respond ok";

    if (Status::NOT_VERIFY == pdu->status) {    // 未验证
        emit error("Clinet must need login");
        return;
    }

    if (Status::SUCCESS == pdu->status) {
        uint32_t cnt = 0;
        memcpy((char*)&cnt, pdu->msg.data(), sizeof(cnt));   // 将文件数目拷贝到file_cnt_
        file_cnt_.store(ntohl(cnt));    // 转换字节序后保存
    }
    else {  // 没有文件，或错误
        file_cnt_.store(0);
    }

    if (!file_vet_) {   // 不存在（基本不可能），则创建
        file_vet_ = std::make_shared<std::vector<FileInfo>>(0);
    }
    file_vet_->clear(); // 清空，（不清空也行，保险）

    // 等待接收文件，交给handleFileInfo
}

void ShortTaskManager::handleMakeDir(std::shared_ptr<PDURespond> pdu) {
    // 处理服务器的回复
    if (Status::NOT_VERIFY == pdu->status) {    // 未验证
        emit error("Clinet must need login");
        return;
    }

    if (Status::SUCCESS == pdu->status) {
        uint64_t new_id = 0;
        uint64_t parent_id = 0;
        memcpy((char*)&new_id, pdu->msg.data(), sizeof(new_id));    // 从响应中提取文件夹 ID
        memcpy((char*)&parent_id, pdu->msg.data()+sizeof(new_id), sizeof(parent_id));
        // !!!!!!!!!!!!!!!!!! 没有处理编码问题，可能有问题 !!!!!!!!!!!!!!!!!!!!!!!!!
        // !!!!!!!!!!!!!!!!!!!! 这里直接使用长度 100 不利于维护与更改，后续可在msg中添加长度 !!!!!!!!!!!!!!!!!!!!!!!!!!
        QString new_dir_name = QString::fromUtf8(pdu->msg.data()+sizeof(new_id)+sizeof(parent_id));
        new_id = ntohll(new_id);    // 转换字节序
        parent_id = ntohll(parent_id);

        emit createDirOK(new_id, parent_id, new_dir_name);  // 创建成功
        qDebug() << "recv make dir respond ok";
    }
    else {
        emit error("Create directory error");               // 处理创建失败的情况
    }
}

void ShortTaskManager::handleDeleteFile(std::shared_ptr<PDURespond> pdu) {
    // 判断响应码
    if (Status::NOT_VERIFY == pdu->status) {    // 未验证
        emit error("Client must need login");
        return;
    }

    if (Status::SUCCESS == pdu->status) {
        emit delFileOK();
        qDebug() << "recv delete file respond ok";
    }
    else {
        emit error("delete file error");
    }
}

void ShortTaskManager::handleFileInfo(std::shared_ptr<FileInfo> pdu) {
    // 开始逐一接受文件信息
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (!file_vet_) {   // 不存在file_vet_
        return;
    }
    if (0 == file_cnt_) {   // 没有执行CD就到来数据或数据接收完，直接过滤
        return;
    }

    file_cnt_ -= 1;
    file_vet_->push_back(*pdu);

    if (0 == file_cnt_) {   // 如果此时接收的是最后一个文件
        //将其按照文件等级从小到达排序
        std::sort(file_vet_->begin(), file_vet_->end(), [](FileInfo& a, FileInfo& b) {return a.dir_grade < b.dir_grade; });
        emit getFileListOK(file_vet_);  // 发送信号，更新文件视图系统
        // 创建新的file_vet_，因为之前的file_vet_交给文件视图系统管理了
        file_vet_ = std::make_shared<std::vector<FileInfo>>(0);
    }

    qDebug() << "recv file info respond ok";
}

void ShortTaskManager::initSignals() {
    // 连接sr_tool_的信号
    // connect(sr_tool_.get(), &SR_Tool::recvPDUOK,
    //         this, &ShortTaskManager::handleRecvPDU);

    connect(sr_tool_.get(), &SR_Tool::recvPDURespondOK,
            this, &ShortTaskManager::handleRecvPDURespond);

    // connect(sr_tool_.get(), &SR_Tool::recvTranPduOK,
    //         this, &ShortTaskManager::handleRecvTranPdu);

    // connect(sr_tool_.get(), &SR_Tool::recvTranDataPduOK,
    //         this, &ShortTaskManager::handleRecvTranDataPdu);

    // connect(sr_tool_.get(), &SR_Tool::recvTranFinishPduOK,
    //         this, &ShortTaskManager::handleRecvTranFinishPdu);

    // connect(sr_tool_.get(), &SR_Tool::recvRespondPackOK,
    //         this, &ShortTaskManager::handleRecvPDURespond);

    // connect(sr_tool_.get(), &SR_Tool::recvUserInfoOK,
    //         this, &ShortTaskManager::handleRecvUserInfo);

    connect(sr_tool_.get(), &SR_Tool::recvFileInfoOK,
            this, &ShortTaskManager::handleRecvFileInfo);
}
