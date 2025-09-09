#include "DiskClient.h"
#include <QApplication>
#include <QStyleFactory>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion"));    // 使用浅色主题
    qRegisterMetaType<FileInfo>("FileInfo");    //将FileInfo注册到元对象系统，方法QVariant使用
    qRegisterMetaType<ItemDate>("ItemDate");
    DiskClient client;
    if (client.startClient()) {
        client.show();
        return a.exec();
    }
    else {
        return 0;
    }
    return a.exec();
}
