QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = StockWatch
TEMPLATE = app

SOURCES += main.cpp mainwindow.cpp klinechart.cpp klinedialog.cpp
HEADERS += mainwindow.h klinechart.h klinedialog.h

# 注：FORMS 已移除，界面全部由代码构建
# MinGW 下源码保存为 UTF-8 即可，无需 /utf-8 参数

