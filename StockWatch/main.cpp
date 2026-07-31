#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 全局 Tooltip 深色文字（浅底 + 深字）
    a.setStyleSheet("QToolTip{color:#151a2a;background:#f0f2f5;border:1px solid #bbb;padding:4px 8px;}");
    MainWindow w;
    w.show();
    return a.exec();
}

