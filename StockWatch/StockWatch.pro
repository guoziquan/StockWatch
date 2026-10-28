QT += core gui network concurrent
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
TARGET = StockWatch
TEMPLATE = app
RC_ICONS = resource/Image/enter_icon.ico
SOURCES += main.cpp mainwindow.cpp klinechart.cpp stockchartwindow.cpp \
           indicator.cpp reportgenerator.cpp autoclosemsg.cpp riskscanner.cpp \
           holdingcalc.cpp tradingcalendar.cpp pricelimit.cpp \
           timesharedata.cpp timesharechart.cpp sectordata.cpp sectordialog.cpp
HEADERS += mainwindow.h klinechart.h stockchartwindow.h \
           indicator.h reportgenerator.h autoclosemsg.h riskscanner.h \
           holdingcalc.h tradingcalendar.h pricelimit.h \
           timesharedata.h timesharechart.h sectordata.h sectordialog.h
