#include "mainwindow.h"
#include "stockchartwindow.h"
#include "reportgenerator.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QSettings>
#include <QTime>
#include <QDateTime>
#include <QColor>
#include <QStandardPaths>
#include <QFile>
#include <QMouseEvent>
#include <QEvent>

// ---------- 常量 ----------
static const QString API_HOST = "https://push2.eastmoney.com/api/qt/ulist.np/get";

static QStringList loadDefaultStocks()
{
    QFile f(QStringLiteral("stocks.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {"002555", "002558", "002602", "002517", "603444", "300533"};

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QJsonArray arr = doc.object().value("stocks").toArray();
    QStringList result;
    for (const QJsonValue &v : arr) {
        QString code = v.toString().trimmed();
        if (code.size() == 6)
            result << code;
    }
    return result.isEmpty()
        ? QStringList{"002555", "002558", "002602", "002517", "603444", "300533"}
        : result;
}
static const QColor COLOR_UP(0xff, 0x4d, 0x5e);
static const QColor COLOR_DOWN(0x2e, 0xe6, 0xa8);
static const QColor COLOR_FLAT(0x8f, 0xa3, 0xc0);
static const QColor COLOR_BG(0x0b, 0x12, 0x20);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    loadConfig();
    buildUi();
    updateMarketStatus();

    // 定时刷新
    m_timer->setInterval(m_currentIntervalSecs * 1000);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::refresh);
    m_timer->start();

    // 倒计时
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        m_countdownValue--;
        if (m_countdownValue < 0) m_countdownValue = 0;
        m_countdownLabel->setText(
            QStringLiteral("下次刷新: %1秒")
                .arg(m_countdownValue, 2, 10, QChar('0')));
    });
    m_countdownTimer->start();

    // 报告生成器 + 午夜调度
    m_reporter = new ReportGenerator(m_net, this);
    connect(m_reporter, &ReportGenerator::reportReady, this, [this](const QString &path) {
        m_statusLabel->setText(QStringLiteral("报告已生成: %1").arg(path));
    });
    connect(m_reporter, &ReportGenerator::allDone, this, [this](int count) {
        m_statusLabel->setText(QStringLiteral("今日报告生成完毕，共%1份").arg(count));
    });

    m_lastReportDate = QDate::currentDate();
    m_reportTimer = new QTimer(this);
    m_reportTimer->setInterval(60 * 1000);
    connect(m_reportTimer, &QTimer::timeout, this, [this]() {
        QDate today = QDate::currentDate();
        if (today > m_lastReportDate) {
            generateReports();
            m_lastReportDate = today;
        }
    });
    m_reportTimer->start();

    refresh();
}

// ---------- 事件过滤器：代码/名称列手型光标 ----------
bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_table->viewport() && ev->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent *>(ev);
        QModelIndex idx = m_table->indexAt(me->pos());
        if (idx.isValid() && (idx.column() == 0 || idx.column() == 1))
            m_table->viewport()->setCursor(Qt::PointingHandCursor);
        else
            m_table->viewport()->setCursor(Qt::ArrowCursor);
    }
    return QMainWindow::eventFilter(obj, ev);
}

// ==================== UI 构建 ====================

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("冰川 · A股行情监控"));
    resize(1080, 640);
    setMinimumSize(940, 540);

    auto *central = new QWidget(this);
    central->setStyleSheet(QString("background:%1;color:#e8eefc;").arg(COLOR_BG.name()));
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(14, 10, 14, 8);
    root->setSpacing(6);

    // ===== ① 顶部摘要栏 =====
    auto *topBar = new QHBoxLayout;
    topBar->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("冰川 · A股行情监控"), central);
    title->setStyleSheet("font-size:17px;font-weight:bold;color:#4da3ff;");

    auto makeIdxLabel = [&](const QString &prefix) -> QLabel * {
        auto *lbl = new QLabel(prefix + QStringLiteral("  --"), central);
        lbl->setStyleSheet("color:#8fa3c0;font-size:12px;");
        lbl->setTextFormat(Qt::RichText);
        return lbl;
    };
    m_idxSh = makeIdxLabel(QStringLiteral("上证"));
    m_idxSz = makeIdxLabel(QStringLiteral("深证"));
    m_idxCy = makeIdxLabel(QStringLiteral("创业板"));

    m_mktLabel = new QLabel(QStringLiteral("--"), central);
    m_mktLabel->setStyleSheet("color:#8fa3c0;font-size:12px;");

    topBar->addWidget(title);
    topBar->addSpacing(8);
    topBar->addWidget(m_idxSh);
    topBar->addWidget(m_idxSz);
    topBar->addWidget(m_idxCy);
    topBar->addStretch();
    topBar->addWidget(m_mktLabel);

    auto *topWidget = new QWidget(central);
    topWidget->setLayout(topBar);
    topWidget->setFixedHeight(40);
    root->addWidget(topWidget);

    // ===== ② 操作工具栏 =====
    auto *toolRow = new QHBoxLayout;
    toolRow->setSpacing(8);

    m_codeEdit = new QLineEdit(central);
    m_codeEdit->setPlaceholderText(QStringLiteral("输入6位代码，如 002555"));
    m_codeEdit->setFixedWidth(180);
    m_codeEdit->setStyleSheet(
        "QLineEdit{background:#131f33;border:1px solid #22334d;border-radius:6px;"
        "padding:7px 10px;color:white;font-size:13px;}"
        "QLineEdit:focus{border-color:#4da3ff;}");
    connect(m_codeEdit, &QLineEdit::returnPressed, this, &MainWindow::addStock);

    auto *addBtn = new QPushButton(QStringLiteral("添加"), central);
    addBtn->setStyleSheet("QPushButton{background:#3d7bff;color:white;border:none;"
                          "border-radius:6px;padding:8px 18px;font-size:13px;}"
                          "QPushButton:hover{background:#2a5fd6;}");
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addStock);

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), central);
    refreshBtn->setStyleSheet("QPushButton{background:#131f33;color:white;border:1px solid #22334d;"
                              "border-radius:6px;padding:8px 18px;font-size:13px;}"
                              "QPushButton:hover{background:#182742;border-color:#3d7bff;}");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refresh);

    auto *intervalLabel = new QLabel(QStringLiteral("刷新间隔:"), central);
    intervalLabel->setStyleSheet("color:#8fa3c0;font-size:13px;");
    m_intervalCombo = new QComboBox(central);
    m_intervalCombo->addItems({QStringLiteral("5秒"), QStringLiteral("10秒"),
                               QStringLiteral("30秒"), QStringLiteral("60秒"),
                               QStringLiteral("120秒")});
    m_intervalCombo->setCurrentIndex(1);
    m_intervalCombo->setStyleSheet(
        "QComboBox{background:#131f33;border:1px solid #22334d;border-radius:6px;"
        "padding:6px 10px;color:white;font-size:13px;}"
        "QComboBox:hover{border-color:#3d7bff;}"
        "QComboBox QAbstractItemView{background:#131f33;color:white;selection-background-color:#2a5fd6;}");
    connect(m_intervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onIntervalChanged);

    m_countdownLabel = new QLabel(
        QStringLiteral("下次刷新: %1秒").arg(m_countdownValue, 2, 10, QChar('0')), central);
    m_countdownLabel->setStyleSheet("color:#8fa3c0;font-size:12px;");
    m_countdownLabel->setMinimumWidth(140);

    toolRow->addWidget(m_codeEdit);
    toolRow->addWidget(addBtn);
    toolRow->addWidget(refreshBtn);
    toolRow->addSpacing(12);
    toolRow->addWidget(intervalLabel);
    toolRow->addWidget(m_intervalCombo);
    toolRow->addWidget(m_countdownLabel);
    toolRow->addStretch();

    auto *toolWidget = new QWidget(central);
    toolWidget->setLayout(toolRow);
    toolWidget->setFixedHeight(44);
    root->addWidget(toolWidget);

    // ===== ③ 表格区域 =====
    m_table = new QTableWidget(0, 10, central);
    m_table->setHorizontalHeaderLabels({QStringLiteral("代码"), QStringLiteral("名称"),
        QStringLiteral("最新价"), QStringLiteral("涨跌幅"), QStringLiteral("涨跌额"),
        QStringLiteral("今开"), QStringLiteral("最高"), QStringLiteral("最低"),
        QStringLiteral("昨收"), QStringLiteral("预警")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(32);
    m_table->setShowGrid(false);
    m_table->setMouseTracking(true);
    m_table->viewport()->installEventFilter(this);
    m_table->setStyleSheet(
        "QTableWidget{background:#131f33;border:1px solid #22334d;border-radius:6px;"
        "color:#e8eefc;font-size:13px;gridline-color:transparent;}"
        "QTableWidget::item{padding:4px 6px;border-bottom:1px solid #1a2740;}"
        "QHeaderView::section{background:#1a2740;color:#c8d6e5;border:none;"
        "padding:8px 4px;font-size:12px;font-weight:bold;}"
        "QTableWidget::item:selected{background:#3a5a8c;color:white;}");
    for (int c = 0; c < 10; ++c) {
        auto *hi = m_table->horizontalHeaderItem(c);
        if (hi) hi->setForeground(QColor(0xc8, 0xd6, 0xe5));
    }
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c < 10; ++c)
        m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    root->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            &MainWindow::onTableDoubleClicked);

    // 右键菜单
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QModelIndex idx = m_table->indexAt(pos);
        if (!idx.isValid()) return;
        m_table->selectRow(idx.row());
        QMenu menu(this);
        menu.addAction(QStringLiteral("🔔 设置价格预警"), this, &MainWindow::setAlert);
        menu.addAction(QStringLiteral("📊 生成今日报告"), this, &MainWindow::generateReports);
        menu.addAction(QStringLiteral("🗑 删除该股票"), this, &MainWindow::removeStock);
        menu.exec(m_table->viewport()->mapToGlobal(pos));
    });

    // ===== ④ 底部状态栏 =====
    m_statusLabel = new QLabel(QStringLiteral("就绪"), central);
    m_statusLabel->setStyleSheet("color:#8fa3c0;font-size:11px;padding:2px 4px;");
    m_statusLabel->setFixedHeight(24);
    root->addWidget(m_statusLabel);
}

// ==================== 数据获取 ====================

QString MainWindow::secId(const QString &code) const
{
    return code.startsWith('6') ? "1." + code : "0." + code;
}

void MainWindow::refresh()
{
    if (m_stocks.isEmpty()) return;
    m_countdownValue = m_currentIntervalSecs;
    fetchQuotes();
}

void MainWindow::fetchQuotes()
{
    QStringList secids;
    for (const QString &c : m_stocks)
        secids << secId(c);
    secids << "1.000001" << "0.399001" << "0.399006";

    QUrl url(API_HOST);
    QUrlQuery q;
    q.addQueryItem("fltt", "2");
    q.addQueryItem("secids", secids.join(','));
    q.addQueryItem("fields", "f2,f3,f4,f12,f14,f15,f16,f17,f18");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onReplyFinished(reply); });
}

void MainWindow::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText(QStringLiteral("获取失败: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject dataObj = doc.object().value("data").toObject();
    QJsonArray diff = dataObj.value("diff").toArray();

    for (const QJsonValue &v : diff) {
        QJsonObject o = v.toObject();
        QString code = o.value("f12").toString();
        if (code.isEmpty()) continue;
        QMap<QString, QString> m;
        m["name"]     = o.value("f14").toString();
        m["price"]    = o.value("f2").toVariant().toString();
        m["chgPct"]   = o.value("f3").toVariant().toString();
        m["chg"]      = o.value("f4").toVariant().toString();
        m["open"]     = o.value("f17").toVariant().toString();
        m["high"]     = o.value("f15").toVariant().toString();
        m["low"]      = o.value("f16").toVariant().toString();
        m["prevClose"]= o.value("f18").toVariant().toString();
        m_quotes[code] = m;
    }

    // 更新大盘指数
    auto fmtIdx = [&](const QString &code, QLabel *lbl, const QString &title) {
        if (m_quotes.contains(code)) {
            const auto &q = m_quotes[code];
            double pct = q["chgPct"].toDouble();
            QColor c = pct > 0 ? COLOR_UP : (pct < 0 ? COLOR_DOWN : COLOR_FLAT);
            lbl->setText(QStringLiteral("<span style='color:#8fa3c0;'>%1 </span>"
                                         "<span style='color:#e8eefc;'>%2 </span>"
                                         "<span style='color:%3;'>%4%</span>")
                             .arg(title, q["price"], c.name(),
                                  (pct > 0 ? "+" : "") + q["chgPct"]));
        }
    };
    fmtIdx("000001", m_idxSh, QStringLiteral("上证"));
    fmtIdx("399001", m_idxSz, QStringLiteral("深证"));
    fmtIdx("399006", m_idxCy, QStringLiteral("创业板"));

    renderTable();
    updateStatusBar();
    checkAlerts();
    updateMarketStatus();
}

// ==================== 表格渲染 ====================

void MainWindow::renderTable()
{
    m_rowOfCode.clear();
    int row = 0;
    for (const QString &code : m_stocks) {
        if (!m_quotes.contains(code)) continue;
        const auto &q = m_quotes[code];
        if (m_table->rowCount() <= row)
            m_table->insertRow(row);

        double pct = q["chgPct"].toDouble();
        QColor color = pct > 0 ? COLOR_UP : (pct < 0 ? COLOR_DOWN : COLOR_FLAT);

        auto setItem = [&](int col, const QString &text, bool colored = false) {
            auto *it = new QTableWidgetItem(text);
            it->setTextAlignment(Qt::AlignCenter);
            if (colored) it->setForeground(color);
            if (col >= 2 && col <= 8 && colored)
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(row, col, it);
        };

        setItem(0, code);
        setItem(1, q["name"]);
        setItem(2, q["price"], true);
        setItem(3, (pct > 0 ? "+" : "") + q["chgPct"] + "%", true);
        setItem(4, (q["chg"].toDouble() > 0 ? "+" : "") + q["chg"], true);
        setItem(5, q["open"]);
        setItem(6, q["high"]);
        setItem(7, q["low"]);
        setItem(8, q["prevClose"]);

        // 预警列 — ↑上破价 ↓下破价
        QString alertText;
        if (m_alerts.contains(code)) {
            auto a = m_alerts[code];
            if (a.contains("up"))
                alertText += QStringLiteral("↑") + QString::number(a["up"], 'f', 2);
            if (a.contains("down"))
                alertText += (alertText.isEmpty() ? QString() : QStringLiteral(" "))
                           + QStringLiteral("↓") + QString::number(a["down"], 'f', 2);
        }
        auto *alertItem = new QTableWidgetItem(alertText.isEmpty() ? QStringLiteral("—") : alertText);
        alertItem->setTextAlignment(Qt::AlignCenter);
        alertItem->setForeground(COLOR_FLAT);
        m_table->setItem(row, 9, alertItem);

        m_rowOfCode[code] = row;
        ++row;
    }
    while (m_table->rowCount() > row)
        m_table->removeRow(m_table->rowCount() - 1);
}

// ==================== 状态栏汇总 ====================

void MainWindow::updateStatusBar()
{
    QString timeStr = QTime::currentTime().toString("HH:mm:ss");
    int total = m_stocks.size();
    int upCount = 0, downCount = 0;
    for (const QString &code : m_stocks) {
        if (m_quotes.contains(code)) {
            double pct = m_quotes[code]["chgPct"].toDouble();
            if (pct > 0) upCount++;
            else if (pct < 0) downCount++;
        }
    }
    m_statusLabel->setText(
        QStringLiteral("%1只自选 | 上涨%2只 | 下跌%3只 | 更新于 %4 | 双击股票查看K线，右键设置预警或删除")
            .arg(total).arg(upCount).arg(downCount).arg(timeStr));
}

// ==================== 预警 ====================

void MainWindow::checkAlerts()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_alerts.begin(); it != m_alerts.end(); ++it) {
        const QString &code = it.key();
        if (!m_quotes.contains(code)) continue;
        double price = m_quotes[code]["price"].toDouble();
        double up = it.value().value("up", -1);
        double down = it.value().value("down", -1);
        const QString &name = m_quotes[code]["name"];

        auto fire = [&](const QString &key, double target, const QString &dir) {
            if (now - m_triggered.value(key, 0) > 5 * 60 * 1000) {
                m_triggered[key] = now;
                QApplication::beep();
                QMessageBox::warning(this, QStringLiteral("📢 股票预警"),
                    QStringLiteral("%1(%2) 已%3 %4，现价 %5")
                        .arg(name, code, dir).arg(target).arg(price));
            }
        };

        if (up > 0 && price >= up)
            fire(code + "_up", up, QStringLiteral("上破"));
        if (down > 0 && price <= down)
            fire(code + "_down", down, QStringLiteral("下破"));
    }
}

void MainWindow::setAlert()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    QString code = m_table->item(row, 0)->text();
    QString name = m_table->item(row, 1)->text();
    QMap<QString, double> cur = m_alerts.value(code);

    bool ok1 = false, ok2 = false;
    double up = QInputDialog::getDouble(this, QStringLiteral("设置预警"),
        QStringLiteral("%1(%2)\n上破价(0=不设):").arg(name, code),
        cur.value("up", 0), 0, 100000, 2, &ok1);
    if (!ok1) return;
    double down = QInputDialog::getDouble(this, QStringLiteral("设置预警"),
        QStringLiteral("%1(%2)\n下破价(0=不设):").arg(name, code),
        cur.value("down", 0), 0, 100000, 2, &ok2);
    if (!ok2) return;

    QMap<QString, double> a;
    if (up > 0)   a["up"] = up;
    if (down > 0) a["down"] = down;
    if (a.isEmpty()) m_alerts.remove(code);
    else             m_alerts[code] = a;

    saveConfig();
    QMessageBox::information(this, QStringLiteral("已保存"),
        QStringLiteral("%1(%2) 预警已保存").arg(name, code));
}

void MainWindow::removeStock()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    QString code = m_table->item(row, 0)->text();
    if (QMessageBox::question(this, QStringLiteral("删除"),
            QStringLiteral("确定从自选列表删除 %1 吗？").arg(code)) != QMessageBox::Yes)
        return;
    m_stocks.removeAll(code);
    m_alerts.remove(code);
    m_quotes.remove(code);
    saveConfig();
    renderTable();
}

// ==================== 添加与定时 ====================

void MainWindow::addStock()
{
    QString code = m_codeEdit->text().trimmed();
    if (code.size() != 6 || !code[0].isDigit() || !code[5].isDigit() ||
        std::any_of(code.begin(), code.end(), [](QChar c){ return !c.isDigit(); })) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请输入6位数字股票代码，如 002555"));
        return;
    }
    if (m_stocks.contains(code)) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("该股票已在自选列表中"));
        return;
    }
    m_stocks.append(code);
    saveConfig();
    m_codeEdit->clear();
    refresh();
}

void MainWindow::onIntervalChanged(int index)
{
    int secs[] = {5, 10, 30, 60, 120};
    m_currentIntervalSecs = secs[qBound(0, index, 4)];
    m_timer->setInterval(m_currentIntervalSecs * 1000);
    m_countdownValue = m_currentIntervalSecs;
}

void MainWindow::updateMarketStatus()
{
    QDateTime now = QDateTime::currentDateTime();
    int wd = now.date().dayOfWeek();
    int hm = now.time().hour() * 60 + now.time().minute();
    QString txt;

    if (wd >= 6) {
        int days = 8 - wd;
        txt = QStringLiteral("休市(周末) · %1天后周一 9:30 开盘").arg(days);
    } else if (hm < 9 * 60 + 15) {
        if (hm < 9 * 60 + 30)
            txt = QStringLiteral("未开盘 · 今日 9:30 开盘");
        else
            txt = QStringLiteral("集合竞价 · 9:30 开盘");
    } else if (hm <= 15 * 60) {
        if ((hm >= 9 * 60 + 30 && hm <= 11 * 60 + 30) ||
            (hm >= 13 * 60 && hm <= 15 * 60)) {
            int remain = 15 * 60 - hm;
            int h = remain / 60, m = remain % 60;
            txt = QStringLiteral("🟢 交易中 · %1:%2后收盘")
                .arg(h).arg(m, 2, 10, QChar('0'));
        } else if (hm < 9 * 60 + 30) {
            txt = QStringLiteral("集合竞价 · 9:30 开盘");
        } else {
            int remain = 13 * 60 - hm;
            int h = remain / 60, m = remain % 60;
            txt = QStringLiteral("☕ 午间休市 · %1:%2后开盘")
                .arg(h).arg(m, 2, 10, QChar('0'));
        }
    } else {
        int daysToNext = 1;
        if (wd == 5) daysToNext = 3;
        txt = QStringLiteral("🔴 已收盘 · %1天后 9:30 开盘").arg(daysToNext);
    }
    m_mktLabel->setText(txt);
}

// ==================== 配置持久化 ====================

void MainWindow::saveConfig()
{
    QSettings s(QStringLiteral("config.ini"), QSettings::IniFormat);
    s.beginWriteArray(QStringLiteral("stocks"));
    for (int i = 0; i < m_stocks.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("code"), m_stocks.at(i));
    }
    s.endArray();

    s.beginGroup(QStringLiteral("alerts"));
    for (auto it = m_alerts.begin(); it != m_alerts.end(); ++it) {
        s.beginGroup(it.key());
        s.setValue(QStringLiteral("up"), it.value().value("up", 0.0));
        s.setValue(QStringLiteral("down"), it.value().value("down", 0.0));
        s.endGroup();
    }
    s.endGroup();
    s.sync();
}

void MainWindow::loadConfig()
{
    QSettings s(QStringLiteral("config.ini"), QSettings::IniFormat);

    int n = s.beginReadArray(QStringLiteral("stocks"));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        QString code = s.value(QStringLiteral("code")).toString();
        if (code.size() == 6) m_stocks << code;
    }
    s.endArray();
    if (m_stocks.isEmpty())
        m_stocks = loadDefaultStocks();

    s.beginGroup(QStringLiteral("alerts"));
    for (const QString &key : s.childGroups()) {
        s.beginGroup(key);
        QMap<QString, double> a;
        double up = s.value(QStringLiteral("up"), 0.0).toDouble();
        double down = s.value(QStringLiteral("down"), 0.0).toDouble();
        if (up > 0)   a["up"] = up;
        if (down > 0) a["down"] = down;
        if (!a.isEmpty()) m_alerts[key] = a;
        s.endGroup();
    }
    s.endGroup();
}

void MainWindow::onTableDoubleClicked(int row, int /*column*/)
{
    if (row < 0 || row >= m_stocks.size()) return;
    QString code = m_stocks.at(row);
    QString name = m_quotes.value(code).value("name", code);
    auto *window = new StockChartWindow(code, name, m_net, this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

// ==================== 报告生成 ====================

void MainWindow::generateReports()
{
    QMap<QString, QString> names;
    for (const QString &code : m_stocks)
        names[code] = m_quotes.value(code).value("name", code);

    m_statusLabel->setText(QStringLiteral("正在生成今日报告..."));
    m_reporter->generateAll(m_stocks, names);
}
