#include "mainwindow.h"
#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyleSheet("QToolTip{color:#151a2a;background:#f0f2f5;border:1px solid #bbb;padding:4px 8px;}");

    // 单实例检测
    static QSharedMemory shm("StockWatch_SingleInstance_Key");
    if (!shm.create(1)) {
        QMessageBox::information(nullptr, QStringLiteral("冰川 · A股行情监控"),
                                 QStringLiteral("软件已在运行中，请查看系统托盘图标"));
        return 0;
    }

    MainWindow w;
    w.show();
    return a.exec();
}

