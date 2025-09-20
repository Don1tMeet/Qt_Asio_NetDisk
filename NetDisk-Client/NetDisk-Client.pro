QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 添加 Boost 头文件路径
INCLUDEPATH += "D:/boost_1_88_0"
# 添加 Boost 库路径
LIBS += -L"D:/boost_1_88_0/stage/lib"
# 链接必要的库
LIBS += -lboost_system-mgw14-mt-x64-1_88
LIBS += -lboost_thread-mgw14-mt-x64-1_88
# 添加网络库
LIBS += -lws2_32
LIBS += -lwsock32

# 添加 OpenSSL 头文件和库路径
INCLUDEPATH += "D:/OpenSSL/include"
LIBS += -L"D:/OpenSSL/lib64" -lcrypto -lssl

# 启用 SSL 支持
CONFIG += ssl

CONFIG += c++20

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += ToolClass
INCLUDEPATH += WidgetClass
INCLUDEPATH += BufferPool

SOURCES += \
    BufferPool/BufferPool.cpp \
    DiskClient.cpp \
    ToolClass/DownTool.cpp \
    ToolClass/SR_Tool.cpp \
    ToolClass/Serializer.cpp \
    ToolClass/ShortTaskManager.cpp \
    ToolClass/TaskQue.cpp \
    ToolClass/UdTool.cpp \
    WidgetClass/FileSystem.cpp \
    WidgetClass/FileViewSystem.cpp \
    WidgetClass/Login.cpp \
    WidgetClass/TProgress.cpp \
    WidgetClass/UserInfoWidget.cpp \
    main.cpp

HEADERS += \
    BufferPool/BufferPool.h \
    DisallowCopyAndMove.h \
    DiskClient.h \
    ToolClass/DownTool.h \
    ToolClass/SR_Tool.h \
    ToolClass/Serializer.h \
    ToolClass/ShortTaskManager.h \
    ToolClass/TaskQue.h \
    ToolClass/UdTool.h \
    WidgetClass/FileSystem.h \
    WidgetClass/FileViewSystem.h \
    WidgetClass/Login.h \
    WidgetClass/TProgress.h \
    WidgetClass/UserInfoWidget.h \
    protocol.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

FORMS +=

RESOURCES += \
    DiskClient.qrc
