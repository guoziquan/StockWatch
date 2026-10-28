#include "mainwindow.h"
#include "stockchartwindow.h"
#include "reportgenerator.h"
#include "autoclosemsg.h"
#include "tradingcalendar.h"
#include "pricelimit.h"
#include "sectordialog.h"

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
#include <QScrollArea>
#include <QFrame>
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
#include <QTextStream>
#include <QTextCodec>
#include <QMouseEvent>
#include <QEvent>
#include <QCloseEvent>
#include <QDialog>
#include <QPointer>
#include <QStyle>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTimer>

// 写运行日志（错误/网络异常等），避免频繁弹窗打扰
static void logMsg(const QString &msg)
{
    QString path = QCoreApplication::applicationDirPath() + "/StockWatch.log";
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
       << "  " << msg << "\n";
    f.close();
}

static QString loadReportDir()
{
    QString jsonFile = QCoreApplication::applicationDirPath() + "/stocks.json";
    QFile f(jsonFile);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QString dir = doc.object().value("reportDir").toString().trimmed();
    return dir.isEmpty() ? QString() : dir;
}

// ---------- 常量 ----------
static const QString API_HOST = "https://push2.eastmoney.com/api/qt/ulist.np/get";

static QStringList loadDefaultStocks()
{
    QString jsonFile = QCoreApplication::applicationDirPath() + "/stocks.json";
    QFile f(jsonFile);
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

// ==================== 持仓计算辅助 ====================
// 实际计算全部委托给 HoldingCalc（移动加权平均 + A 股费用模型），
// 详见 holdingcalc.h。这里只保留入口封装。

int MainWindow::netShares(const QString &code) const
{
    return HoldingCalc::compute(code, m_holdings.value(code), 0.0, m_feeConfig).netShares;
}

HoldingSummary MainWindow::holdingSummary(const QString &code, double currentPrice) const
{
    return HoldingCalc::compute(code, m_holdings.value(code), currentPrice, m_feeConfig);
}

HoldingSummary MainWindow::holdingSummaryNow(const QString &code) const
{
    const double price = m_quotes.value(code).value("price", "0").toDouble();
    return holdingSummary(code, price);
}

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

    // 启动即拉取一次，避免首屏空白且要等一个完整刷新周期。
    // 不用 singleShot(0)：构造函数阶段事件循环尚未接管，回调时机不可靠。
    // 改由 showEvent 首次触发（见 MainWindow::showEvent）。

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
    QString rd = loadReportDir();
    if (rd.isEmpty())
        rd = QCoreApplication::applicationDirPath() + "/reports";
    m_riskScanner = new RiskScanner(m_net, this);
    connect(m_riskScanner, &RiskScanner::riskReady, this, &MainWindow::onRiskResult);
    m_reporter = new ReportGenerator(m_net, m_riskScanner, this);
    m_reporter->setReportDir(rd);
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

    // ---- 系统托盘 ----
    m_trayIcon = new QSystemTrayIcon(this);
    QIcon icon(QCoreApplication::applicationDirPath() + "/enter_icon.ico");
    if (icon.isNull())
        icon = qApp->style()->standardIcon(QStyle::SP_ComputerIcon);
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip(QStringLiteral("冰川 · A股行情监控"));

    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(
        "QMenu{background:#131f33;color:#e8eefc;border:1px solid #22334d;padding:4px;}"
        "QMenu::item{padding:6px 24px;}"
        "QMenu::item:selected{background:#2a5fd6;}"
        "QMenu::separator{background:#22334d;height:1px;margin:4px 8px;}");
    m_trayMenu->addAction(QStringLiteral("显示主窗口"), this, [this]{
        show();
        raise();
        activateWindow();
    });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(QStringLiteral("退出"), qApp, &QApplication::quit);
    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            if (isVisible()) hide(); else { show(); raise(); activateWindow(); }
        }
    });
    m_trayIcon->show();

    refresh();
}

// ---------- 事件过滤器：代码/名称列手型光标 ----------
bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_table->viewport()) {
        if (ev->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            QModelIndex idx = m_table->indexAt(me->pos());
            if (idx.isValid() && (idx.column() == 0 || idx.column() == 1))
                m_table->viewport()->setCursor(Qt::PointingHandCursor);
            else
                m_table->viewport()->setCursor(Qt::ArrowCursor);
        } else if (ev->type() == QEvent::MouseButtonDblClick) {
            // 双击打开 K 线：只允许代码/名称列，与手型光标提示严格一致。
            // 其他列双击不做任何事（持仓列有自己的按钮，避免误触冲突）。
            QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::LeftButton) {
                const QModelIndex idx = m_table->indexAt(me->pos());
                if (idx.isValid() && (idx.column() == 0 || idx.column() == 1)) {
                    onTableDoubleClicked(idx.row(), idx.column());
                    return true;   // 已处理，阻止默认行为
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

// ---------- 关闭窗口 → 最小化到托盘 ----------
void MainWindow::closeEvent(QCloseEvent *ev)
{
    hide();
    ev->ignore();
}

// ---------- 首次显示 → 立即拉取行情 ----------
void MainWindow::showEvent(QShowEvent *ev)
{
    QMainWindow::showEvent(ev);
    if (m_firstShown) return;
    m_firstShown = true;
    // 此时窗口已显示、事件循环已就绪，网络回调能正常回到 UI 线程。
    if (!m_stocks.isEmpty()) {
        m_idleSkipCount = 4;   // 让首次 refresh 在空闲时段也能通过取模判定
        fetchQuotes();
    }
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
    topBar->setSpacing(0);   // 由下方显式控制间距

    auto *title = new QLabel(QStringLiteral("冰川 · A股行情监控"), central);
    title->setStyleSheet("font-size:17px;font-weight:bold;color:#4da3ff;");

    auto makeIdxLabel = [&](const QString &prefix) -> QLabel * {
        auto *lbl = new QLabel(prefix + QStringLiteral("  --"), central);
        // 指数做成独立色块：深色底 + 圆角，使三个指数在视觉上彼此分离，
        // 避免"名称(灰)与数值(白)与涨跌(红/绿)"混成一条难以扫视的长串。
        lbl->setStyleSheet(
            "QLabel{color:#8fa3c0;font-size:12px;background:#111a2e;"
            "border:1px solid #22334d;border-radius:4px;padding:3px 9px;}");
        lbl->setTextFormat(Qt::RichText);
        return lbl;
    };
    m_idxSh = makeIdxLabel(QStringLiteral("上证"));
    m_idxSz = makeIdxLabel(QStringLiteral("深证"));
    m_idxCy = makeIdxLabel(QStringLiteral("创业板"));

    m_mktLabel = new QLabel(QStringLiteral("--"), central);
    m_mktLabel->setStyleSheet("color:#8fa3c0;font-size:12px;");

    topBar->addWidget(title);
    topBar->addSpacing(14);
    topBar->addWidget(m_idxSh);
    topBar->addSpacing(8);
    topBar->addWidget(m_idxSz);
    topBar->addSpacing(8);
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
                          "border-radius:6px;padding:6px 18px;font-size:13px;min-height:28px;}"
                          "QPushButton:hover{background:#2a5fd6;}");
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addStock);

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), central);
    refreshBtn->setStyleSheet("QPushButton{background:#131f33;color:white;border:1px solid #22334d;"
                              "border-radius:6px;padding:6px 18px;font-size:13px;min-height:28px;}"
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
        "padding:6px 10px;color:white;font-size:13px;min-width:74px;}"
        "QComboBox:hover{border-color:#3d7bff;}"
        // 默认箭头在深色背景下是刺眼的浅色系统三角，与主题不协调。
        // 用 QSS 画一个向下的小箭头：两条 border 拼成 V 形，颜色与辅助文字一致。
        "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:center right;"
        "width:20px;border:none;background:transparent;}"
        "QComboBox::down-arrow{image:none;border-left:4px solid transparent;"
        "border-right:4px solid transparent;border-top:5px solid #8fa3c0;"
        "width:0;height:0;margin-right:6px;}"
        "QComboBox::down-arrow:on{border-top-color:#4da3ff;}"
        "QComboBox QAbstractItemView{background:#131f33;color:white;"
        "border:1px solid #22334d;outline:none;"
        "selection-background-color:#2a5fd6;selection-color:white;padding:2px;}");
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
    toolWidget->setFixedHeight(50);
    root->addWidget(toolWidget);

    // ===== ②½ 造假风险警告栏(红字, 可上下滚动, 无风险时隐藏) =====
    m_riskBar = new QWidget(central);
    m_riskBar->setStyleSheet(
        "QWidget#riskBar{background:#2a1218;border:1px solid #ff4d5e;border-radius:4px;}");
    m_riskBar->setObjectName("riskBar");

    auto *riskLayout = new QHBoxLayout(m_riskBar);
    riskLayout->setContentsMargins(6, 4, 6, 4);
    riskLayout->setSpacing(6);

    // 可滚动区
    auto *riskScroll = new QScrollArea(m_riskBar);
    riskScroll->setWidgetResizable(true);
    riskScroll->setFrameShape(QFrame::NoFrame);
    riskScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    riskScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 压缩高度：风险栏是"提示"而非"主体"，最多占 2 行(~44px)，
    // 超出部分内部滚动。旧值 90px 会挤占表格可视行数。
    riskScroll->setMaximumHeight(46);
    riskScroll->setStyleSheet(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:vertical{background:transparent;width:8px;margin:0;}"
        "QScrollBar::handle:vertical{background:#ff4d5e;border-radius:4px;min-height:16px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}");

    m_riskLabel = new QLabel();
    m_riskLabel->setTextFormat(Qt::RichText);
    m_riskLabel->setWordWrap(true);
    m_riskLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_riskLabel->setStyleSheet(
        "QLabel{color:#ff4d5e;background:transparent;padding:2px 6px;font-size:12px;}");
    riskScroll->setWidget(m_riskLabel);

    // 清理按钮
    auto *clearBtn = new QPushButton(QStringLiteral("清理警告"), m_riskBar);
    clearBtn->setFixedWidth(76);
    clearBtn->setStyleSheet(
        "QPushButton{background:#131f33;color:#ff6b6b;border:1px solid #22334d;"
        "border-radius:4px;padding:4px 6px;font-size:11px;}"
        "QPushButton:hover{background:#182742;border-color:#ff4d5e;}");
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearRiskWarnings);

    riskLayout->addWidget(riskScroll, 1);
    riskLayout->addWidget(clearBtn, 0, Qt::AlignTop);

    m_riskBar->hide();  // 初始无风险，隐藏
    root->addWidget(m_riskBar);

    // ===== ③ 表格区域 =====
    m_table = new QTableWidget(0, 13, central);
    m_table->setHorizontalHeaderLabels({QStringLiteral("代码"), QStringLiteral("名称"),
        QStringLiteral("最新价"), QStringLiteral("涨跌幅"), QStringLiteral("涨跌额"),
        QStringLiteral("今开"), QStringLiteral("最高"), QStringLiteral("最低"),
        QStringLiteral("昨收"), QStringLiteral("涨停价"), QStringLiteral("跌停价"),
        QStringLiteral("预警"), QStringLiteral("持仓")});
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
        "QHeaderView::section{background:#1a2740;color:#151a2a;border:none;"
        "padding:8px 4px;font-size:12px;font-weight:bold;}"
        "QHeaderView::section:hover,QHeaderView::section:pressed{background:#1a2740;color:#151a2a;"
        "border:none;padding:8px 4px;font-size:12px;font-weight:bold;}"
        "QTableWidget::item:selected{background:#3a5a8c;color:white;}");
    for (int c = 0; c < 13; ++c) {
        auto *hi = m_table->horizontalHeaderItem(c);
        if (hi) hi->setForeground(QColor(0x15, 0x1a, 0x2a));
    }
    // 列宽策略（13列）：
    //   0 代码 / 2-10 数值列 -> 自适应内容
    //   1 名称 -> 拉伸填充
    //   12 持仓按钮 -> 固定宽度
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c <= 10; ++c)
        m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(11, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(12, QHeaderView::Fixed);
    m_table->setColumnWidth(12, 86);
    root->addWidget(m_table, 1);

    // 双击打开 K 线窗口统一由 eventFilter 处理（见 MainWindow::eventFilter），
    // 保证与手型光标提示一致；此处不再连接 cellDoubleClicked，避免重复触发。

    // 右键菜单
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QModelIndex idx = m_table->indexAt(pos);
        if (!idx.isValid()) return;
        m_table->selectRow(idx.row());
        QMenu menu(this);
        menu.addAction(QStringLiteral("🔔 设置价格预警"), this, &MainWindow::setAlert);
        menu.addAction(QStringLiteral("📊 生成今日报告"), this, &MainWindow::generateReports);
        menu.addSeparator();
        auto *codeItem = m_table->item(idx.row(), 0);
        auto *nameItem = m_table->item(idx.row(), 1);
        if (!codeItem || !nameItem) return;
        QString code = codeItem->text();
        QString name = nameItem->text();
        // 板块联动：查看所属板块、板块涨幅与相对强度
        menu.addAction(QStringLiteral("🧩 板块联动"), this,
                       [this, code, name]{ openSectorAnalysis(code, name); });
        menu.addAction(QStringLiteral("💰 买入记录"), this, [this, code, name]{ buyStock(code, name); });
        menu.addSeparator();
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

    const QDateTime now = QDateTime::currentDateTime();
    const SessionPhase ph = TradingCalendar::phaseAt(now);

    // 非交易日（含法定节假日）：当日只补刷一次，确认数据日期后停止轮询，
    // 避免在休市日制造"行情在跳动"的错觉。
    if (!TradingCalendar::isTradingDay(now.date())) {
        if (m_lastClosedRefreshDate != now.date()) {
            m_lastClosedRefreshDate = now.date();
            fetchQuotes();
        } else {
            updateStatusBar();
            updateMarketStatus();
        }
        return;
    }

    // 盘前/午休/盘后：降低轮询频率（每 5 个周期一次），仍保持收盘价最终一致
    const bool liveSession = (ph == SessionPhase::MorningTrading
                              || ph == SessionPhase::AfternoonTrading
                              || ph == SessionPhase::CallAuction
                              || ph == SessionPhase::CallAuctionEnd);
    if (!liveSession) {
        if (++m_idleSkipCount % 5 != 0) {
            updateStatusBar();
            updateMarketStatus();
            return;
        }
    }

    fetchQuotes();
}
static QString tencentCode(const QString &code)
{
    return code.startsWith('6') ? "sh" + code : "sz" + code;
}

static QString tencentIdxCode(const QString &code)
{
    // 上证指数 → sh000001, 深证成指 → sz399001, 创业板指 → sz399006
    if (code == "000001") return "sh000001";
    return "sz" + code;
}

void MainWindow::fetchQuotes()
{
    // 缺少 OpenSSL 时 HTTPS 必然失败，直接改用腾讯 HTTP 源（不依赖 SSL）
    if (!QSslSocket::supportsSsl()) {
        m_statusLabel->setText(QStringLiteral("缺少OpenSSL库，改用腾讯源(HTTP)…"));
        fetchTencentQuotes();
        return;
    }

    // 尚无任何行情时给出明确的加载提示，避免用户对着空表格以为卡死
    if (m_quotes.isEmpty())
        m_statusLabel->setText(QStringLiteral("正在加载行情…"));

    m_usingTencent = false;

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
    req.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://quote.eastmoney.com/");
    req.setRawHeader("Accept", "application/json, text/plain, */*");
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onReplyFinished(reply); });

    // 超时保护：15 秒无响应则放弃
    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
}

// ---------- 腾讯行情（HTTP，不依赖 SSL，作为 DNS 劫持时的备选）----------
void MainWindow::fetchTencentQuotes()
{
    m_usingTencent = true;

    QStringList codes;
    for (const QString &c : m_stocks)
        codes << tencentCode(c);
    // 三大指数
    codes << tencentIdxCode("000001")
          << tencentIdxCode("399001")
          << tencentIdxCode("399006");

    QUrl url("http://qt.gtimg.cn/q=" + codes.join(','));
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Mozilla/5.0");
    req.setRawHeader("Referer", "https://gu.qq.com/");

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // 腾讯也失败，回到东方财富重试逻辑
            m_statusLabel->setText(
                QStringLiteral("腾讯源也失败: %1").arg(reply->errorString()));
            if (m_retryCount < 3) {
                m_retryCount++;
                int delays[] = {3, 8, 20};
                QTimer::singleShot(delays[m_retryCount - 1] * 1000, this, [this]() {
                    if (!m_stocks.isEmpty()) {
                        m_statusLabel->setText(
                            QStringLiteral("第%1次重试…").arg(m_retryCount));
                        fetchQuotes();
                    }
                });
            }
            return;
        }

        parseTencentReply(reply->readAll());
        renderTable();
        refreshHoldingDialogs();
        updateStatusBar();
        checkAlerts();
        scanRisks();
        updateMarketStatus();

        m_retryCount = 0;
        m_statusLabel->setText(
            QStringLiteral("行情来自腾讯源 (上次东方财富请求失败)"));
    });

    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
}

void MainWindow::parseTencentReply(const QByteArray &data)
{
    // 格式: v_sz002555="51~三七互娱~002555~20.27~20.18~...
    // 索引: [0]市场 [1]名称 [2]代码 [3]最新价 [4]昨收 [5]今开
    //       [6]成交量 [30]时间 [31]涨跌额 [32]涨跌幅% [33]最高 [34]最低
    //       [37]成交额(万) [38]换手率
    //
    // 编码说明（重要）：qt.gtimg.cn 返回的是 **GBK** 编码，
    // 早期按 UTF-8 解码会导致中文名称乱码（"三七互娱" 变成方块/问号）。
    // 这里统一按 GBK 解码。
    QTextCodec *gbk = QTextCodec::codecForName("GBK");
    if (!gbk) gbk = QTextCodec::codecForName("GB18030");
    QString text = gbk ? gbk->toUnicode(data) : QString::fromLocal8Bit(data);
    QStringList lines = text.split('\n', QString::SkipEmptyParts);

    for (const QString &line : lines) {
        // 提取 = 和 " 之间的值
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        int q1 = line.indexOf('\"', eq);
        int q2 = line.indexOf('\"', q1 + 1);
        if (q1 < 0 || q2 < 0) continue;
        QString body = line.mid(q1 + 1, q2 - q1 - 1);
        QStringList f = body.split('~');
        if (f.size() < 35) continue;

        QString code = f[2];
        QMap<QString, QString> m;
        m["name"]     = f[1];
        m["price"]    = f[3];
        m["chgPct"]   = f[32];
        m["chg"]      = f[31];
        m["open"]     = f[5];
        m["high"]     = f[33];
        m["low"]      = f[34];
        m["prevClose"]= f[4];

        // 非交易时段兜底
        double priceVal = m["price"].toDouble();
        double prevVal  = m["prevClose"].toDouble();
        if (priceVal <= 0.0 && prevVal > 0.0) {
            m["price"]  = QString::number(prevVal, 'f', 2);
            m["chgPct"] = "0.00";
            m["chg"]    = "0.00";
            m["open"]   = m["prevClose"];
            m["high"]   = m["prevClose"];
            m["low"]    = m["prevClose"];
        }
        m_quotes[code] = m;
    }

    // 更新指数显示
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
}

void MainWindow::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString errMsg = reply->errorString();
        int errCode = reply->error();

        // 错误只写日志 + 状态栏提示，不再弹窗打扰
        if (errMsg.contains("SSL", Qt::CaseInsensitive)
            || errMsg.contains("TLS", Qt::CaseInsensitive)
            || errMsg.contains("handshake", Qt::CaseInsensitive)) {
            m_statusLabel->setText(QStringLiteral("获取失败: SSL握手失败"));
            logMsg(QStringLiteral("[行情] SSL握手失败: %1").arg(errMsg));
        } else if (errMsg.contains("Connection closed", Qt::CaseInsensitive)
                   || errMsg.contains("Connection refused", Qt::CaseInsensitive)
                   || errMsg.contains("Host", Qt::CaseInsensitive)
                   || errMsg.contains("timed out", Qt::CaseInsensitive)
                   || errMsg.contains("Timeout", Qt::CaseInsensitive)) {
            m_statusLabel->setText(QStringLiteral("获取失败: 无法连接行情服务器"));
            logMsg(QStringLiteral("[行情] 网络错误: %1").arg(errMsg));
        } else if (errCode == QNetworkReply::OperationCanceledError) {
            m_statusLabel->setText(QStringLiteral("获取失败: 请求超时"));
            logMsg(QStringLiteral("[行情] 请求超时: %1").arg(errMsg));
        } else {
            m_statusLabel->setText(QStringLiteral("获取失败: %1 (错误码:%2)")
                                   .arg(errMsg).arg(errCode));
            logMsg(QStringLiteral("[行情] 错误(%2): %1").arg(errMsg).arg(errCode));
        }

        // 东方财富失败后，优先切腾讯源尝试一次
        if (!m_usingTencent) {
            m_statusLabel->setText(QStringLiteral("东方财富不通，尝试腾讯源…"));
            // 首次加载（还没有任何行情）时立即回退，缩短空窗期；
            // 已有数据时保持 1 秒，避免网络抖动导致频繁切换。
            const int delayMs = m_quotes.isEmpty() ? 0 : 1000;
            QTimer::singleShot(delayMs, this, [this]() {
                if (!m_stocks.isEmpty()) fetchTencentQuotes();
            });
        } else if (m_retryCount < 3) {
            // 腾讯也失败，回退到东方财富重试
            m_retryCount++;
            int delays[] = {3, 8, 20};  // 秒
            int delay = delays[m_retryCount - 1];
            QTimer::singleShot(delay * 1000, this, [this, delay]() {
                if (!m_stocks.isEmpty()) {
                    m_statusLabel->setText(
                        QStringLiteral("第%1次重试…").arg(m_retryCount));
                    fetchQuotes();
                }
            });
        }
        return;
    }

    // 请求成功，重置重试计数
    m_retryCount = 0;

    const QByteArray body = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(body);
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

        // 非交易时段 API 只返回昨收，实时字段为 0 — 用昨收回退
        double priceVal = m["price"].toDouble();
        double prevVal  = m["prevClose"].toDouble();
        if (priceVal <= 0.0 && prevVal > 0.0) {
            m["price"]  = QString::number(prevVal, 'f', 2);
            m["chgPct"] = "0.00";
            m["chg"]    = "0.00";
            m["open"]   = QString::number(prevVal, 'f', 2);
            m["high"]   = QString::number(prevVal, 'f', 2);
            m["low"]    = QString::number(prevVal, 'f', 2);
        }
        m_quotes[code] = m;
    }

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
    refreshHoldingDialogs();
    updateStatusBar();
    checkAlerts();
    scanRisks();
    updateMarketStatus();
    // 风险结果可能早于行情返回（名称缺失），行情到达后重绘一次警告栏
    updateRiskLabel();
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

        // 涨停价 / 跌停价：按板块与风险警示状态精确计算
        const double prevClose = q["prevClose"].toDouble();
        const double curPrice  = q["price"].toDouble();
        const PriceLimit pl = PriceLimitCalc::compute(code, q["name"], prevClose);
        if (pl.valid) {
            // 封板时用实心色块强调，便于一眼识别
            const bool atUp   = PriceLimitCalc::isLimitUp(curPrice, pl);
            const bool atDown = PriceLimitCalc::isLimitDown(curPrice, pl);

            auto *upItem = new QTableWidgetItem(
                QString::number(pl.upPrice, 'f', 2));
            upItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            upItem->setForeground(atUp ? QColor(0xff, 0xff, 0xff) : COLOR_UP);
            if (atUp)
                upItem->setBackground(QColor(0xff, 0x4d, 0x5e));
            upItem->setToolTip(pl.explanation);
            m_table->setItem(row, 9, upItem);

            auto *dnItem = new QTableWidgetItem(
                QString::number(pl.downPrice, 'f', 2));
            dnItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            dnItem->setForeground(atDown ? QColor(0xff, 0xff, 0xff) : COLOR_DOWN);
            if (atDown)
                dnItem->setBackground(QColor(0x2e, 0xe6, 0xa8));
            dnItem->setToolTip(pl.explanation);
            m_table->setItem(row, 10, dnItem);
        } else {
            for (int c = 9; c <= 10; ++c) {
                auto *it = new QTableWidgetItem(QStringLiteral("—"));
                it->setTextAlignment(Qt::AlignCenter);
                it->setForeground(COLOR_FLAT);
                m_table->setItem(row, c, it);
            }
        }

        // 预警列
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
        m_table->setItem(row, 11, alertItem);

        // 持仓按钮列：复用已有按钮，避免每次刷新重建（原实现存在泄漏）
        int net = netShares(code);
        auto *btn = qobject_cast<QPushButton *>(m_table->cellWidget(row, 12));
        const QString wantText = (net > 0) ? QStringLiteral("持仓数据") : QStringLiteral("＋买入");
        if (!btn) {
            btn = new QPushButton(wantText);
            btn->setStyleSheet(
                "QPushButton{background:#131f33;color:#c8d6e5;border:1px solid #22334d;"
                "border-radius:4px;padding:3px 10px;font-size:11px;}"
                "QPushButton:hover{background:#182742;border-color:#3d7bff;color:white;}");
            const QString cap = code;
            connect(btn, &QPushButton::clicked, this, [this, cap]{ openHoldingDetail(cap); });
            m_table->setCellWidget(row, 12, btn);
        } else if (btn->text() != wantText) {
            btn->setText(wantText);
        }

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
        QStringLiteral("%1只自选 | 上涨%2只 | 下跌%3只 | 更新于 %4 | 双击代码/名称查看K线，右键设置预警或删除")
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
                AutoCloseMessageBox::warn(this, QStringLiteral("📢 股票预警"),
                    QStringLiteral("%1(%2) 已%3 %4，现价 %5")
                        .arg(name, code, dir).arg(target).arg(price), 5);
            }
        };

        if (up > 0 && price >= up)
            fire(code + "_up", up, QStringLiteral("上破"));
        if (down > 0 && price <= down)
            fire(code + "_down", down, QStringLiteral("下破"));
    }

    checkDropAlert();
}

// ==================== 暴跌预警（分档：跌超5%提醒 + 逼近跌停预警） ====================

// 涨跌停幅度按板块/状态区分
double MainWindow::limitPct(const QString &code, const QString &name) const
{
    // ST / *ST 股：无论板块，统一 ±5%
    if (name.contains(QStringLiteral("ST"), Qt::CaseInsensitive))
        return 5.0;
    // 北交所（8/4 开头）：±30%
    if (code.startsWith('8') || code.startsWith('4'))
        return 30.0;
    // 创业板（300）/ 科创板（688）：±20%
    if (code.startsWith('3') || code.startsWith("688"))
        return 20.0;
    // 主板（60/00 开头）：±10%
    return 10.0;
}

void MainWindow::checkDropAlert()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 COOLDOWN = 30 * 60 * 1000;  // 30 分钟冷却，避免盘中反复弹

    for (const QString &code : m_stocks) {
        if (!m_quotes.contains(code)) continue;
        const auto &q = m_quotes[code];
        const QString name = q["name"];
        double pct = q["chgPct"].toDouble();
        if (pct >= 0) continue;  // 只盯下跌

        double drop = -pct;                 // 跌幅（正数）
        double limit = limitPct(code, name);

        // 两档判断：逼近跌停优先，其次跌超 5%
        QString key;
        QString msg;
        if (limit - drop < 1.0) {           // 距跌停 < 1 个百分点 → 跌停预警
            key = code + "_limitdown";
            msg = QStringLiteral("%1(%2) 逼近跌停，现跌幅 -%3%（跌停 -%4%）")
                      .arg(name, code)
                      .arg(drop, 0, 'f', 2)
                      .arg(limit, 0, 'f', 0);
        } else if (drop >= 5.0) {           // 跌幅 ≥ 5% → 大跌提醒
            key = code + "_drop5";
            msg = QStringLiteral("%1(%2) 大跌，现跌幅 -%3%")
                      .arg(name, code)
                      .arg(drop, 0, 'f', 2);
        } else {
            continue;
        }

        if (now - m_triggered.value(key, 0) > COOLDOWN) {
            m_triggered[key] = now;
            QApplication::beep();
            AutoCloseMessageBox::warn(this, QStringLiteral("📉 暴跌预警"), msg, 5);
        }
    }
}

// ==================== 板块联动与相对强度 ====================

// 抓取个股所属板块（东财 slist 接口，返回板块代码/名称/实时涨幅）
void MainWindow::fetchSectors(const QString &code)
{
    QUrl url(QStringLiteral(
        "https://push2.eastmoney.com/api/qt/slist/get"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("spt"), QStringLiteral("3"));
    q.addQueryItem(QStringLiteral("fltt"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("invt"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("fields"), QStringLiteral("f12,f13,f14,f3"));
    q.addQueryItem(QStringLiteral("secid"), secId(code));
    q.addQueryItem(QStringLiteral("pn"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("pz"), QStringLiteral("30"));
    q.addQueryItem(QStringLiteral("po"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("np"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("ut"), QStringLiteral("b2884a393a59ad64002292a3e90d46a5"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://quote.eastmoney.com/");
    req.setRawHeader("Accept", "application/json, text/plain, */*");
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    QNetworkReply *reply = m_net->get(req);
    reply->setProperty("stockCode", code);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onSectorReply(reply); });

    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
}

void MainWindow::onSectorReply(QNetworkReply *reply)
{
    reply->deleteLater();
    const QString code = reply->property("stockCode").toString();
    if (code.isEmpty()) return;

    if (reply->error() != QNetworkReply::NoError) {
        logMsg(QStringLiteral("[板块] %1 获取失败: %2")
                   .arg(code, reply->errorString()));
        // 失败时也让窗口显示"数据不足"，而不是一直转圈
        updateSectorDialogs(code);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray diff = doc.object().value(QStringLiteral("data")).toObject()
                               .value(QStringLiteral("diff")).toArray();

    QVector<SectorItem> sectors;
    for (const QJsonValue &v : diff) {
        const QJsonObject o = v.toObject();
        SectorItem s;
        s.code = o.value(QStringLiteral("f12")).toString();
        s.name = o.value(QStringLiteral("f14")).toString();
        const QJsonValue pv = o.value(QStringLiteral("f3"));
        s.changePct = pv.isString() ? pv.toString().toDouble()
                                    : pv.toVariant().toDouble();
        if (!s.valid()) continue;
        s.kind = SectorCalc::kindOf(s.code, s.name);
        sectors.append(s);
    }

    const QString name = m_quotes.value(code).value("name", code);
    const double stockPct = m_quotes.value(code).value("chgPct", "0").toDouble();

    m_sectorLinks[code] = SectorCalc::build(code, name, stockPct, sectors);
    updateSectorDialogs(code);
}

// 构建/刷新板块分析窗口（窗口 UI 由 SectorDialog 负责）
void MainWindow::openSectorAnalysis(const QString &code, const QString &name)
{
    if (m_sectorDialogs.contains(code) && m_sectorDialogs[code]) {
        m_sectorDialogs[code]->raise();
        m_sectorDialogs[code]->activateWindow();
        updateSectorDialogs(code);
        return;
    }

    auto *dlg = new SectorDialog(code, name, this);
    m_sectorDialogs[code] = dlg;
    connect(dlg, &QDialog::destroyed, this, [this, code]{
        m_sectorDialogs.remove(code);
    });
    dlg->show();

    // 有缓存直接渲染；否则发起请求
    if (m_sectorLinks.contains(code))
        updateSectorDialogs(code);
    else
        fetchSectors(code);
}

// 数据到达后刷新板块窗口
void MainWindow::updateSectorDialogs(const QString &code)
{
    if (!m_sectorDialogs.contains(code) || !m_sectorDialogs[code]) return;
    auto *dlg = qobject_cast<SectorDialog *>(m_sectorDialogs[code]);
    if (!dlg) return;

    if (!m_sectorLinks.contains(code)) {
        dlg->setPending(QStringLiteral("板块数据获取失败或仍在加载…"));
        return;
    }
    dlg->setLink(m_sectorLinks.value(code));
}



void MainWindow::scanRisks()
{
    if (m_stocks.isEmpty()) return;
    QMap<QString, QString> names;
    for (const QString &code : m_stocks)
        names[code] = m_quotes.value(code).value("name", code);
    m_riskScanner->scanAll(m_stocks, names);
}

void MainWindow::onRiskResult(const QString &code, const RiskResult &result)
{
    // 缓存结果，供警告栏持续显示（不再弹窗，改为常驻红字提示）
    m_riskResults[code] = result;
    updateRiskLabel();
}

// 更新造假风险警告栏：红字列出所有有风险的股票，无风险则隐藏
void MainWindow::updateRiskLabel()
{
    QStringList riskyCodes;
    QStringList gaps;
    for (auto it = m_riskResults.begin(); it != m_riskResults.end(); ++it) {
        if (it.value().hasRisk)
            riskyCodes << it.key();
        // 收集数据缺口（仅对无风险的股票提示，避免干扰风险信息）
        if (!it.value().hasRisk) {
            for (const QString &g : it.value().dataGaps) {
                const QString line = QStringLiteral("%1(%2)：%3")
                                         .arg(it.value().name, it.key(), g);
                if (!gaps.contains(line)) gaps << line;
                break;   // 每只股票只提示一条主要缺口
            }
        }
    }

    if (riskyCodes.isEmpty()) {
        m_riskLabel->clear();
        // 只有在确实扫描过才提示数据覆盖情况
        if (!gaps.isEmpty() && !m_riskResults.isEmpty()) {
            m_riskLabel->setText(
                QStringLiteral("<span style='color:#8fa3c0;'>风险扫描已完成，但存在数据覆盖不足：</span><br/>")
                + QStringLiteral("<span style='color:#8fa3c0;'>%1</span>")
                      .arg(gaps.mid(0, 3).join(QStringLiteral("<br/>"))));
            m_riskBar->show();
            return;
        }
        m_riskBar->hide();
        return;
    }

    QStringList parts;
    QStringList tooltipParts;
    for (const QString &code : riskyCodes) {
        const RiskResult &r = m_riskResults[code];
        // 单行摘要：股票名 + 最短的风险描述
        QString hint;
        if (!r.redFlags.isEmpty())
            hint = r.redFlags.first();
        else if (!r.hits.isEmpty())
            hint = r.hits.first();
        // 名称可能尚未从行情获取到（扫描早于首次行情返回），
        // 此时只显示代码，避免出现 "301117(301117)" 这类重复。
        const QString label = (r.name.isEmpty() || r.name == code)
                                  ? code
                                  : QStringLiteral("%1(%2)").arg(r.name, code);
        parts << QStringLiteral("<b>%1</b> %2").arg(label, hint);

        // 完整详情进 tooltip
        QString detail = label;
        for (const QString &f : r.redFlags) detail += QStringLiteral("\n· ") + f;
        for (const QString &h : r.hits)    detail += QStringLiteral("\n· ") + h;
        tooltipParts << detail;
    }
    m_riskLabel->setText(QStringLiteral("⚠ 风险预警：") + parts.join(QStringLiteral("　｜　")));
    m_riskLabel->setToolTip(tooltipParts.join(QStringLiteral("\n\n")));
    m_riskBar->show();
}

// 清理警告：清空缓存并隐藏警告栏
void MainWindow::clearRiskWarnings()
{
    m_riskResults.clear();
    m_riskLabel->clear();
    m_riskBar->hide();
}

void MainWindow::setAlert()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    auto *codeItem = m_table->item(row, 0);
    auto *nameItem = m_table->item(row, 1);
    if (!codeItem || !nameItem) return;
    QString code = codeItem->text();
    QString name = nameItem->text();
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
    AutoCloseMessageBox::info(this, QStringLiteral("已保存"),
        QStringLiteral("%1(%2) 预警已保存").arg(name, code), 5);
}

void MainWindow::removeStock()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    auto *codeItem = m_table->item(row, 0);
    if (!codeItem) return;
    QString code = codeItem->text();
    if (QMessageBox::question(this, QStringLiteral("删除"),
            QStringLiteral("确定从自选列表删除 %1 吗？").arg(code)) != QMessageBox::Yes)
        return;
    m_stocks.removeAll(code);
    m_alerts.remove(code);
    m_quotes.remove(code);
    m_holdings.remove(code);
    // 关闭该股票的持仓明细窗口
    if (m_holdingWidgets.contains(code)) {
        const auto &w = m_holdingWidgets.take(code);
        if (w.dialog) w.dialog->close();
    }
    saveConfig();
    renderTable();
}

// ==================== 添加与定时 ====================

void MainWindow::addStock()
{
    QString code = m_codeEdit->text().trimmed();
    if (code.size() != 6 || !code[0].isDigit() || !code[5].isDigit() ||
        std::any_of(code.begin(), code.end(), [](QChar c){ return !c.isDigit(); })) {
        AutoCloseMessageBox::warn(this, QStringLiteral("提示"),
                             QStringLiteral("请输入6位数字股票代码，如 002555"), 5);
        return;
    }
    if (m_stocks.contains(code)) {
        AutoCloseMessageBox::info(this, QStringLiteral("提示"),
                                 QStringLiteral("该股票已在自选列表中"), 5);
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
    // 交由 TradingCalendar 统一判断：含法定节假日与盘中分段
    const QDateTime now = QDateTime::currentDateTime();
    QString txt = TradingCalendar::describe(now);

    // 交易日历未覆盖当年时，明确提示，避免休市日被当成交易日
    if (!TradingCalendar::coversYear(now.date().year()))
        txt += QStringLiteral("（节假日库未更新）");

    m_mktLabel->setText(txt);
}

// ==================== 配置持久化 ====================

void MainWindow::saveConfig()
{
    QSettings s(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
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

    // 持仓持久化
    s.beginWriteArray(QStringLiteral("holdings"));
    int hi = 0;
    for (auto it = m_holdings.begin(); it != m_holdings.end(); ++it) {
        for (const auto &rec : it.value()) {
            s.setArrayIndex(hi++);
            s.setValue(QStringLiteral("code"), rec.code);
            s.setValue(QStringLiteral("isBuy"), rec.isBuy);
            s.setValue(QStringLiteral("shares"), rec.shares);
            s.setValue(QStringLiteral("price"), rec.price);
            s.setValue(QStringLiteral("date"), rec.date.toString("yyyy-MM-dd"));
        }
    }
    s.endArray();

    s.sync();
}

void MainWindow::loadConfig()
{
    QSettings s(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);

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

    // 持仓加载
    n = s.beginReadArray(QStringLiteral("holdings"));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        HoldingRecord rec;
        rec.code   = s.value(QStringLiteral("code")).toString();
        rec.isBuy  = s.value(QStringLiteral("isBuy"), true).toBool();
        rec.shares = s.value(QStringLiteral("shares"), 0).toInt();
        rec.price  = s.value(QStringLiteral("price"), 0.0).toDouble();
        rec.date   = QDate::fromString(s.value(QStringLiteral("date")).toString(), "yyyy-MM-dd");
        if (!rec.code.isEmpty() && rec.shares > 0)
            m_holdings[rec.code].append(rec);
    }
    s.endArray();
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

// ==================== 持仓：买入 / 卖出 / 明细窗口 ====================

void MainWindow::buyStock(const QString &code, const QString &name)
{
    double defaultPrice = m_quotes.value(code).value("price", "0").toDouble();

    bool ok1 = false;
    int shares = QInputDialog::getInt(this, QStringLiteral("买入"),
        QStringLiteral("%1(%2)\n买入股数:").arg(name, code),
        100, 1, 100000000, 1, &ok1);
    if (!ok1) return;

    bool ok2 = false;
    double price = QInputDialog::getDouble(this, QStringLiteral("买入"),
        QStringLiteral("%1(%2)\n每股价格:").arg(name, code),
        defaultPrice, 0.01, 100000, 2, &ok2);
    if (!ok2) return;

    HoldingRecord rec;
    rec.code   = code;
    rec.isBuy  = true;
    rec.shares = shares;
    rec.price  = price;
    rec.date   = QDate::currentDate();
    m_holdings[code].append(rec);

    saveConfig();
    renderTable();

    // 如果明细窗口开着，就地刷新
    if (m_holdingWidgets.contains(code))
        refreshHoldingDialog(code);
}

void MainWindow::sellStock(const QString &code, const QString &name)
{
    int net = netShares(code);
    if (net <= 0) {
        AutoCloseMessageBox::info(this, QStringLiteral("提示"), QStringLiteral("当前无持仓，无法卖出"), 5);
        return;
    }

    double defaultPrice = m_quotes.value(code).value("price", "0").toDouble();

    bool ok1 = false;
    int shares = QInputDialog::getInt(this, QStringLiteral("卖出"),
        QStringLiteral("%1(%2)\n当前持仓 %3 股\n卖出股数:").arg(name, code).arg(net),
        net, 1, net, 1, &ok1);
    if (!ok1) return;

    bool ok2 = false;
    double price = QInputDialog::getDouble(this, QStringLiteral("卖出"),
        QStringLiteral("%1(%2)\n卖出价格:").arg(name, code),
        defaultPrice, 0.01, 100000, 2, &ok2);
    if (!ok2) return;

    HoldingRecord rec;
    rec.code   = code;
    rec.isBuy  = false;
    rec.shares = shares;
    rec.price  = price;
    rec.date   = QDate::currentDate();
    m_holdings[code].append(rec);

    saveConfig();
    renderTable();

    // 如果明细窗口开着，就地刷新
    if (m_holdingWidgets.contains(code))
        refreshHoldingDialog(code);
}

void MainWindow::openHoldingDetail(const QString &code)
{
    // 已经开着就不重复创建
    if (m_holdingWidgets.contains(code) && m_holdingWidgets[code].dialog) return;

    QString name = m_quotes.value(code).value("name", code);

    auto *dlg = new QDialog(this, Qt::Window);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QStringLiteral("%1 (%2) — 持仓明细").arg(name, code));
    dlg->resize(620, 460);
    dlg->setMinimumSize(520, 360);
    dlg->setStyleSheet("background:#0b1220;color:#e8eefc;");

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(12, 12, 12, 10);
    root->setSpacing(8);

    // ---- 汇总头部 ----
    auto *header = new QFrame(dlg);
    header->setStyleSheet("QFrame{background:#0f1829;border:1px solid #22334d;border-radius:6px;}");
    header->setFixedHeight(52);
    auto *hLayout = new QVBoxLayout(header);
    hLayout->setContentsMargins(14, 6, 14, 6);

    auto *row1 = new QHBoxLayout;
    auto *netLabel     = new QLabel(header); row1->addWidget(netLabel);
    auto *avgLabel     = new QLabel(header); row1->addWidget(avgLabel);
    auto *buyCostLabel = new QLabel(header); row1->addWidget(buyCostLabel);
    auto *sellLabel    = new QLabel(header); row1->addWidget(sellLabel);
    auto *curPriceLabel= new QLabel(header); row1->addWidget(curPriceLabel);
    row1->addStretch();
    hLayout->addLayout(row1);

    auto *pnlLabel = new QLabel(header);
    hLayout->addWidget(pnlLabel);
    root->addWidget(header);

    // ---- 交易记录表格 ----
    auto *table = new QTableWidget(0, 5, dlg);
    table->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("操作"),
        QStringLiteral("股数"), QStringLiteral("价格"), QStringLiteral("金额")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(false);
    table->setStyleSheet(
        "QTableWidget{background:#131f33;border:1px solid #22334d;border-radius:4px;"
        "color:#e8eefc;font-size:12px;}"
        "QTableWidget::item{padding:3px 6px;border-bottom:1px solid #1a2740;}"
        "QHeaderView::section{background:#1a2740;color:#151a2a;border:none;padding:6px 4px;font-size:11px;}"
        "QHeaderView::section:hover,QHeaderView::section:pressed{background:#1a2740;color:#151a2a;"
        "border:none;padding:6px 4px;font-size:11px;}");
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0, 110);  // 日期
    table->setColumnWidth(1, 50);   // 操作
    table->setColumnWidth(2, 60);   // 股数
    table->setColumnWidth(3, 70);   // 价格
    for (int c = 0; c < 4; ++c)
        table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Interactive);
    for (int c = 0; c < 5; ++c) {
        auto *hi = table->horizontalHeaderItem(c);
        if (hi) hi->setForeground(QColor(0x15, 0x1a, 0x2a));
    }
    root->addWidget(table, 1);

    // ---- 底部操作栏 ----
    auto *btnRow = new QHBoxLayout;

    auto *buyBtn = new QPushButton(QStringLiteral("＋ 买入"), dlg);
    buyBtn->setStyleSheet("QPushButton{background:#3d7bff;color:white;border:none;border-radius:4px;"
                          "padding:6px 16px;font-size:12px;}QPushButton:hover{background:#2a5fd6;}");
    connect(buyBtn, &QPushButton::clicked, this, [this, code, name]{ buyStock(code, name); });

    auto *sellBtn = new QPushButton(QStringLiteral("- 卖出"), dlg);
    sellBtn->setStyleSheet("QPushButton{background:#131f33;color:white;border:1px solid #22334d;border-radius:4px;"
                           "padding:6px 16px;font-size:12px;}QPushButton:hover{background:#182742;border-color:#3d7bff;}");
    connect(sellBtn, &QPushButton::clicked, this, [this, code, name]{ sellStock(code, name); });

    auto *delBtn = new QPushButton(QStringLiteral("删除"), dlg);
    delBtn->setStyleSheet("QPushButton{background:#131f33;color:#ff6b6b;border:1px solid #22334d;border-radius:4px;"
                          "padding:6px 16px;font-size:12px;}QPushButton:hover{background:#182742;border-color:#ff4d5e;}");
    connect(delBtn, &QPushButton::clicked, this, [this, code, table]{
        int row = table->currentRow();
        if (row < 0) {
            AutoCloseMessageBox::info(this, QStringLiteral("提示"), QStringLiteral("请先在表格中选中要删除的记录"), 5);
            return;
        }
        if (!m_holdings.contains(code)) return;
        const auto &v = m_holdings[code];
        if (row < 0 || row >= v.size()) return;
        HoldingRecord r = v[row];  // 值拷贝
        QString detail = QStringLiteral("%1 %2股 @%3")
            .arg(r.isBuy ? QStringLiteral("买入") : QStringLiteral("卖出"))
            .arg(r.shares).arg(r.price, 0, 'f', 2);
        if (QMessageBox::question(this, QStringLiteral("确认删除"),
                QStringLiteral("确定删除这条记录吗？\n%1\n\n注：删除后不可恢复").arg(detail)) != QMessageBox::Yes)
            return;
        m_holdings[code].removeAt(row);
        if (m_holdings[code].isEmpty()) m_holdings.remove(code);
        saveConfig();
        renderTable();
        // 就地刷新，窗口保留不关闭
        refreshHoldingDialog(code);
    });

    btnRow->addWidget(buyBtn);
    btnRow->addWidget(sellBtn);
    btnRow->addStretch();
    btnRow->addWidget(delBtn);
    root->addLayout(btnRow);

    // 缓存控件引用
    HoldingWidgets w;
    w.dialog       = dlg;
    w.netLabel     = netLabel;
    w.avgLabel     = avgLabel;
    w.buyCostLabel = buyCostLabel;
    w.sellLabel    = sellLabel;
    w.curPriceLabel= curPriceLabel;
    w.pnlLabel     = pnlLabel;
    w.table        = table;
    m_holdingWidgets[code] = w;

    // dl g关闭时清理
    connect(dlg, &QDialog::destroyed, this, [this, code]{
        m_holdingWidgets.remove(code);
    });

    refreshHoldingDialog(code);
    dlg->show();
}

// 就地更新持仓明细窗口（不关旧建新）
void MainWindow::refreshHoldingDialog(const QString &code)
{
    if (!m_holdingWidgets.contains(code)) return;
    HoldingWidgets &w = m_holdingWidgets[code];
    if (!w.dialog) { m_holdingWidgets.remove(code); return; }

    HoldingSummary s = holdingSummaryNow(code);
    w.curPrice = m_quotes.value(code).value("price", "0").toDouble();

    auto lblStyle = QStringLiteral("color:#8fa3c0;font-size:12px;border:none;background:transparent;");

    w.netLabel->setStyleSheet(lblStyle);
    w.netLabel->setText(QStringLiteral("净持仓: %1 股").arg(s.netShares));
    // 成本价用移动加权平均的真实持仓成本(含买入费用),而非历次买入均价
    w.avgLabel->setStyleSheet(lblStyle);
    w.avgLabel->setText(QStringLiteral("│  成本: %1")
        .arg(s.netShares > 0 ? QString::number(s.costPrice, 'f', 3) : QStringLiteral("--")));
    w.buyCostLabel->setStyleSheet(lblStyle);
    w.buyCostLabel->setText(QStringLiteral("│  总买入: %1")
        .arg(s.totalBuyCost > 0 ? QString::number(s.totalBuyCost, 'f', 2) : QStringLiteral("--")));
    w.sellLabel->setStyleSheet(lblStyle);
    w.sellLabel->setText(QStringLiteral("│  总卖出: %1")
        .arg(s.totalSellIncome > 0 ? QString::number(s.totalSellIncome, 'f', 2) : QStringLiteral("--")));
    w.curPriceLabel->setStyleSheet(lblStyle);
    w.curPriceLabel->setText(QStringLiteral("│  当前: %1")
        .arg(w.curPrice > 0 ? QString::number(w.curPrice, 'f', 2) : QStringLiteral("--")));

    // 第二行：已实现盈亏 / 浮动盈亏 / 累计盈亏(均含费用)
    auto fmtPnl = [](double v) {
        return QStringLiteral("%1%2").arg(v >= 0 ? "+" : "").arg(v, 0, 'f', 2);
    };
    auto pnlColorOf = [](double v) {
        return v > 0 ? COLOR_UP : (v < 0 ? COLOR_DOWN : COLOR_FLAT);
    };

    if (s.valid && (s.netShares > 0 || s.realizedPnl != 0.0)) {
        QString txt = QStringLiteral("累计盈亏  %1 (%2%)    已实现 %3    浮动 %4    费用 %5")
            .arg(fmtPnl(s.totalPnl))
            .arg(s.totalPnlPct(), 0, 'f', 2)
            .arg(fmtPnl(s.realizedPnl))
            .arg(fmtPnl(s.unrealizedPnl))
            .arg(s.totalFee, 0, 'f', 2);
        w.pnlLabel->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:bold;border:none;background:transparent;")
                .arg(pnlColorOf(s.totalPnl).name()));
        w.pnlLabel->setText(txt);
    } else {
        w.pnlLabel->setStyleSheet(QStringLiteral("color:#8fa3c0;font-size:12px;border:none;background:transparent;"));
        w.pnlLabel->setText(QStringLiteral("无持仓"));
    }

    // 更新表格
    static const QColor H_BUY(0x2e, 0xe6, 0xa8);
    static const QColor H_SELL(0xff, 0x4d, 0x5e);
    const auto &recs = m_holdings.value(code);
    w.table->setRowCount(recs.size());
    for (int i = 0; i < recs.size(); ++i) {
        const auto &r = recs[i];
        QColor c = r.isBuy ? H_BUY : H_SELL;

        auto setCell = [&](int col, const QString &text, const QColor &fg, Qt::Alignment align) {
            auto *item = new QTableWidgetItem(text);
            item->setTextAlignment(align);
            item->setForeground(fg);
            w.table->setItem(i, col, item);
        };

        setCell(0, r.date.toString("yyyy-MM-dd"), COLOR_FLAT, Qt::AlignCenter);
        setCell(1, r.isBuy ? QStringLiteral("买入") : QStringLiteral("卖出"), c, Qt::AlignCenter);
        setCell(2, QString::number(r.shares), c, Qt::AlignRight | Qt::AlignVCenter);
        setCell(3, QString::number(r.price, 'f', 2), c, Qt::AlignRight | Qt::AlignVCenter);
        double amt = r.shares * r.price;
        setCell(4, QStringLiteral("%1%2").arg(r.isBuy ? "+" : "-").arg(amt, 0, 'f', 2), c,
                Qt::AlignRight | Qt::AlignVCenter);
    }
}

// 行情刷新时同步更新所有已打开的持仓明细窗口
void MainWindow::refreshHoldingDialogs()
{
    QStringList codes = m_holdingWidgets.keys();
    for (const QString &code : codes)
        refreshHoldingDialog(code);
}
