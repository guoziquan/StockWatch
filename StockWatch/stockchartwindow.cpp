#include "stockchartwindow.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDateTime>
#include <QTimer>
#include <QSettings>
#include <QApplication>
#include <QDesktopWidget>
#include <QCloseEvent>
#include <QCoreApplication>

static const char *KLINE_API =
    "https://push2his.eastmoney.com/api/qt/stock/kline/get";

// 统一的下拉框样式：深色主题 + 自绘三角箭头
// （系统默认箭头在深色背景下是突兀的浅色方块，与主题不协调）
static QString comboStyle(int minWidth)
{
    return QStringLiteral(
        "QComboBox{background:#131f33;border:1px solid #22334d;border-radius:6px;"
        "padding:5px 10px;color:white;min-width:%1px;font-size:12px;}"
        "QComboBox:hover{border-color:#3d7bff;}"
        "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:center right;"
        "width:20px;border:none;background:transparent;}"
        "QComboBox::down-arrow{image:none;border-left:4px solid transparent;"
        "border-right:4px solid transparent;border-top:5px solid #8fa3c0;"
        "width:0;height:0;margin-right:6px;}"
        "QComboBox::down-arrow:on{border-top-color:#4da3ff;}"
        "QComboBox QAbstractItemView{background:#131f33;color:white;"
        "border:1px solid #22334d;outline:none;"
        "selection-background-color:#2a5fd6;selection-color:white;padding:2px;}")
        .arg(minWidth);
}

StockChartWindow::StockChartWindow(const QString &code, const QString &name,
                                   QNetworkAccessManager *sharedNet,
                                   QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_code(code)
    , m_name(name)
    , m_net(sharedNet)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("%1(%2) - K线图").arg(name, code));
    setMinimumSize(860, 500);
    setStyleSheet(QStringLiteral("background:#0d1422;color:#e8eefc;"));

    // 恢复上次的窗口几何；首次或无记录时用默认尺寸并居中偏上
    QSettings st(QCoreApplication::applicationDirPath() + "/config.ini",
                 QSettings::IniFormat);
    st.beginGroup(QStringLiteral("chartWindow"));
    const QByteArray geo = st.value(QStringLiteral("geometry")).toByteArray();
    const int splitRatio = st.value(QStringLiteral("splitRatio"), 0).toInt();
    st.endGroup();

    bool restored = false;
    if (!geo.isEmpty()) {
        restored = restoreGeometry(geo);
        // 恢复后仍需校验：窗口可能在已拔掉的副屏上，或尺寸异常
        if (restored) {
            const QRect g = geometry();
            const QRect avail = QApplication::desktop()->availableGeometry(g.topLeft());
            if (!avail.intersects(g.adjusted(20, 20, -20, -20))) restored = false;
        }
    }
    if (!restored) {
        resize(1120, 700);
        // 默认位置：屏幕居中偏上，避免与主窗口完全重叠
        if (QApplication::desktop()) {
            const QRect scr = QApplication::desktop()->availableGeometry(this);
            move(scr.center().x() - width() / 2,
                 qMax(scr.top() + 40, scr.center().y() - height() / 2 - 40));
        }
    }

    buildUi();
    if (splitRatio >= 20 && splitRatio <= 85)
        m_chart->setSplitRatio(splitRatio / 100.0);
    fetchKLine();
}

void StockChartWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 6);
    root->setSpacing(4);

    // ---- 顶部工具栏 ----
    auto *topBar = new QHBoxLayout;
    topBar->setSpacing(10);

    m_titleLabel = new QLabel(QStringLiteral("%1 (%2)").arg(m_name, m_code), this);
    m_titleLabel->setStyleSheet("font-size:15px;font-weight:bold;color:#4da3ff;");

    // 周期切换
    m_periodCombo = new QComboBox(this);
    m_periodCombo->addItems({QStringLiteral("分时"), QStringLiteral("60分钟"),
                             QStringLiteral("日K"), QStringLiteral("周K"),
                             QStringLiteral("月K")});
    m_periodCombo->setCurrentIndex(2);
    m_periodCombo->setStyleSheet(comboStyle(72));
    connect(m_periodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StockChartWindow::onPeriodChanged);

    // 主图指标
    m_mainIndCombo = new QComboBox(this);
    m_mainIndCombo->addItems({QStringLiteral("MA"), QStringLiteral("BOLL"), QStringLiteral("无")});
    m_mainIndCombo->setCurrentIndex(0);
    m_mainIndCombo->setStyleSheet(comboStyle(62));
    connect(m_mainIndCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StockChartWindow::onMainIndicatorChanged);

    // 副图指标
    m_subIndCombo = new QComboBox(this);
    m_subIndCombo->addItems({QStringLiteral("成交量"), QStringLiteral("MACD"),
                             QStringLiteral("KDJ"), QStringLiteral("主力资金")});
    m_subIndCombo->setCurrentIndex(0);
    m_subIndCombo->setStyleSheet(comboStyle(72));
    connect(m_subIndCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StockChartWindow::onSubIndicatorChanged);

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    refreshBtn->setStyleSheet(
        "QPushButton{background:#131f33;color:white;border:1px solid #22334d;"
        "border-radius:6px;padding:5px 14px;font-size:12px;}"
        "QPushButton:hover{background:#182742;border-color:#3d7bff;}");
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        if (m_isTimeShare) fetchTimeShare();
        else               fetchKLine();
    });

    topBar->addWidget(m_titleLabel);
    topBar->addStretch();
    topBar->addWidget(m_periodCombo);
    topBar->addWidget(m_mainIndCombo);
    topBar->addWidget(m_subIndCombo);
    topBar->addWidget(refreshBtn);

    // 固定工具栏高度
    auto *topWidget = new QWidget(this);
    topWidget->setLayout(topBar);
    topWidget->setFixedHeight(42);
    root->addWidget(topWidget);

    // ---- 中间主体：左图表（K线/分时可切换） + 右信息面板 ----
    auto *body = new QHBoxLayout;
    body->setSpacing(0);

    m_viewStack = new QStackedWidget(this);

    m_chart = new KLineChart(this);
    connect(m_chart, &KLineChart::hoveredChanged, this,
            [this](const KLineData &kd) {
        int idx = -1;
        for (int i = 0; i < (int)m_cacheData.size(); ++i) {
            if (m_cacheData[i].date == kd.date) { idx = i; break; }
        }
        // 日期匹配失败时，退化为按收盘价查找最接近的一根，
        // 避免 prev=nullptr 导致"昨收 --/涨跌 +收盘价"的错误显示
        if (idx < 0 && !m_cacheData.isEmpty()) {
            for (int i = (int)m_cacheData.size() - 1; i >= 0; --i) {
                if (qFuzzyCompare(m_cacheData[i].close, kd.close)) { idx = i; break; }
            }
        }
        const KLineData *prev = nullptr;
        if (idx > 0 && idx < m_cacheData.size())
            prev = &m_cacheData[idx - 1];
        m_infoPanel->setData(kd, prev);
    });
    m_viewStack->addWidget(m_chart);          // index 0 = K线

    m_tsChart = new TimeShareChart(this);
    m_viewStack->addWidget(m_tsChart);        // index 1 = 分时

    body->addWidget(m_viewStack, 1);

    m_infoPanel = new KLineInfoPanel(this);
    body->addWidget(m_infoPanel);

    root->addLayout(body, 1);

    // ---- 底部状态栏 ----
    m_statusLabel = new QLabel(QStringLiteral("加载中…"), this);
    m_statusLabel->setStyleSheet("color:#8fa3c0;font-size:11px;padding:2px 4px;");
    m_statusLabel->setFixedHeight(24);
    root->addWidget(m_statusLabel);
}

// 关闭前保存窗口几何与主副图比例，下次打开沿用
void StockChartWindow::closeEvent(QCloseEvent *ev)
{
    QSettings st(QCoreApplication::applicationDirPath() + "/config.ini",
                 QSettings::IniFormat);
    st.beginGroup(QStringLiteral("chartWindow"));
    st.setValue(QStringLiteral("geometry"), saveGeometry());
    if (m_chart)
        st.setValue(QStringLiteral("splitRatio"), int(m_chart->splitRatio() * 100));
    st.endGroup();
    st.sync();
    QWidget::closeEvent(ev);
}

QString StockChartWindow::secId() const{
    return m_code.startsWith('6') ? "1." + m_code : "0." + m_code;
}

void StockChartWindow::fetchKLine()
{
    m_triedSina = false;
    m_statusLabel->setText(QStringLiteral("加载中…"));
    // 同时(并行)拉取主力资金流，供"主力资金"副图使用
    fetchMoneyFlow();

    QUrl url(KLINE_API);
    QUrlQuery q;
    q.addQueryItem("secid", secId());
    q.addQueryItem("klt", QString::number(m_klt));
    q.addQueryItem("fqt", "1");
    q.addQueryItem("fields1", "f1,f2,f3,f4,f5,f6");
    q.addQueryItem("fields2", "f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61");
    q.addQueryItem("ut", "b2884a393a59ad64002292a3e90d46a5");
    int lmt = (m_klt == 5 || m_klt == 60) ? 300 : 500;
    q.addQueryItem("lmt", QString::number(lmt));
    q.addQueryItem("end", "20500101");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://quote.eastmoney.com/");
    req.setRawHeader("Accept", "application/json, text/plain, */*");
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onReplyFinished(reply); });
}

void StockChartWindow::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // 东方财富失败 → 尝试新浪
        if (!m_triedSina) {
            m_statusLabel->setText(QStringLiteral("东方财富不通，尝试新浪源…"));
            fetchSinaKLine();
            return;
        }
        m_statusLabel->setText(QStringLiteral("请求失败: %1 (code=%2)")
                               .arg(reply->errorString()).arg(reply->error()));
        return;
    }

    QByteArray body = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonArray kl = doc.object().value("data").toObject().value("klines").toArray();
    if (kl.isEmpty()) {
        // 数据为空也可能是 DNS 劫持返回了无意义内容 → 尝试新浪
        if (!m_triedSina) {
            m_statusLabel->setText(QStringLiteral("东方财富无数据，尝试新浪源…"));
            fetchSinaKLine();
            return;
        }
        m_statusLabel->setText(QStringLiteral("无数据"));
        m_chart->clear();
        return;
    }

    QVector<KLineData> vec;
    vec.reserve(kl.size());
    for (const QJsonValue &v : kl) {
        QStringList parts = v.toString().split(',');
        if (parts.size() < 7) continue;
        KLineData d;
        d.date   = parts[0];
        d.open   = parts[1].toDouble();
        d.close  = parts[2].toDouble();
        d.high   = parts[3].toDouble();
        d.low    = parts[4].toDouble();
        d.volume = parts[5].toDouble();
        if (parts.size() >= 7)  d.amount    = parts[6].toDouble();
        if (parts.size() >= 8)  d.amplitude = parts[7].toDouble();
        if (parts.size() >= 9)  d.chgPct    = parts[8].toDouble();
        if (parts.size() >= 11) d.turnover  = parts[10].toDouble();
        vec.append(d);
    }

    m_cacheData = vec;
    m_chart->setData(vec);
    // K线数据就绪后重新对齐资金流（若资金流先返回则在此渲染）
    m_chart->setMoneyFlow(m_flows);

    int total = vec.size();
    int vis = m_chart->visibleCount();

    if (!vec.isEmpty()) {
        const KLineData *prev = nullptr;
        if (vec.size() >= 2) prev = &vec[vec.size() - 2];
        m_infoPanel->setData(vec.last(), prev);
    }

    int visStart = m_chart->visibleStart();
    int visEnd = qMin(visStart + vis - 1, total - 1);
    QString totalRange = QStringLiteral("%1 ~ %2")
        .arg(vec.first().date.left(10), vec.last().date.left(10));
    QString visRange;
    if (visStart >= 0 && visStart < total && visEnd >= visStart) {
        visRange = QStringLiteral("%1 ~ %2")
            .arg(vec[visStart].date.left(10), vec[visEnd].date.left(10));
    }

    m_statusLabel->setText(
        QStringLiteral("数据量：%1条 | 当前可见：%2条 | 总区间：%3 | 可见区间：%4")
            .arg(total).arg(vis).arg(totalRange, visRange));
}

// ---------- 主力资金流（东方财富 fflow/daykline）----------
// 仅在日K/周K/月K周期下有意义；分钟线不请求，避免无谓流量。
void StockChartWindow::fetchMoneyFlow()
{
    // 分时视图不显示资金流副图，跳过
    if (m_isTimeShare) return;
    // 分钟线周期不支持资金流
    if (m_klt == 5 || m_klt == 60) {
        m_flows.clear();
        m_chart->setMoneyFlow(m_flows);
        return;
    }
    if (m_flowLoading) return;
    m_flowLoading = true;

    QUrl url(QStringLiteral(
        "https://push2his.eastmoney.com/api/qt/stock/fflow/daykline/get"));
    QUrlQuery q;
    q.addQueryItem("lmt", "0");
    q.addQueryItem("klt", "101");
    q.addQueryItem("secid", secId());
    q.addQueryItem("fields1", "f1,f2,f3,f7");
    q.addQueryItem("fields2", "f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61,f62,f63,f64,f65");
    q.addQueryItem("ut", "b2884a393a59ad64002292a3e90d46a5");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://quote.eastmoney.com/");
    req.setRawHeader("Accept", "application/json, text/plain, */*");
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onFlowReplyFinished(reply); });

    // 超时保护
    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
}

void StockChartWindow::onFlowReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    m_flowLoading = false;

    if (reply->error() != QNetworkReply::NoError) {
        m_flows.clear();
        m_chart->setMoneyFlow(m_flows);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray kl = doc.object().value("data").toObject()
                             .value("klines").toArray();

    QVector<FlowPoint> vec;
    vec.reserve(kl.size());
    for (const QJsonValue &v : kl) {
        // 字段: 日期,主力净流入,小单,中单,大单,超大单,主力净占比,...
        const QStringList parts = v.toString().split(',');
        if (parts.size() < 7) continue;
        FlowPoint fp;
        fp.date      = parts[0].left(10);
        fp.mainNet   = parts[1].toDouble();
        fp.mainRatio = parts[6].toDouble();
        vec.append(fp);
    }

    m_flows = vec;
    // 若K线已就绪，立即对齐渲染；否则等待 onReplyFinished/setData 后再应用
    if (!m_cacheData.isEmpty())
        m_chart->setMoneyFlow(m_flows);

    // 若当前正查看主力资金副图，提示数据状态
    if (m_subIndCombo && m_subIndCombo->currentIndex() == 3) {
        if (m_flows.isEmpty())
            m_statusLabel->setText(QStringLiteral("主力资金流暂无数据（可能为停牌或新股）"));
    }
}

// ---------- 新浪 K线（JSON API，作为东方财富备选）----------
void StockChartWindow::fetchSinaKLine()
{
    m_triedSina = true;

    // 新浪 K线 API: symbol=sz002555, scale=240(日K), datalen=500
    QString sinaCode = m_code.startsWith('6') ? "sh" + m_code : "sz" + m_code;

    // 新浪目前只支持日K/周K/月K（分钟线需要不同接口，暂映射到日K）
    int scale = 240;  // 默认日K
    int limit = 500;
    if (m_klt == 102)      { scale = 1200; limit = 250; }  // 周K（新浪无直接支持，给多点日K自己聚合？暂用日K替代标注）
    else if (m_klt == 103) { scale = 7200; limit = 120; }  // 月K

    // 新浪周K/月K的 scale 值不同，实际用 datalen 控制数量
    // 新浪不支持前复权，原数据不带复权信息

    QUrl url("https://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData");
    QUrlQuery q;
    q.addQueryItem("symbol", sinaCode);
    q.addQueryItem("scale", QString::number(scale));
    q.addQueryItem("datalen", QString::number(limit));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://finance.sina.com.cn/");

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onSinaReplyFinished(reply); });
}

void StockChartWindow::onSinaReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText(QStringLiteral("新浪源也失败: %1").arg(reply->errorString()));
        m_chart->clear();
        return;
    }

    // 新浪返回 JSON 数组: [{"day":"2026-08-04","open":"20.290","close":"20.180",...},...]
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray arr = doc.array();
    if (arr.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("新浪无数据"));
        m_chart->clear();
        return;
    }

    QVector<KLineData> vec;
    vec.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        KLineData d;
        d.date   = o.value("day").toString();
        d.open   = o.value("open").toString().toDouble();
        d.close  = o.value("close").toString().toDouble();
        d.high   = o.value("high").toString().toDouble();
        d.low    = o.value("low").toString().toDouble();
        d.volume = o.value("volume").toString().toDouble();
        // 新浪返回的 volume 是股数，需要转为手（/100）
        if (d.volume > 100000) d.volume /= 100;
        // 新浪没有成交额、振幅、换手率等字段，置0
        d.amount = 0;
        d.amplitude = 0;
        d.turnover = 0;
        // 涨跌幅根据 close 和前一日计算，暂不实现（需要缓存前一日的close）
        d.chgPct = 0;
        vec.append(d);
    }

    // 自己算涨跌幅
    for (int i = 1; i < vec.size(); ++i) {
        double prevC = vec[i - 1].close;
        if (prevC > 0) {
            vec[i].chgPct = (vec[i].close - prevC) / prevC * 100.0;
        }
    }

    m_cacheData = vec;
    m_chart->setData(vec);
    // K线数据就绪后重新对齐资金流（若资金流先返回则在此渲染）
    m_chart->setMoneyFlow(m_flows);

    int total = vec.size();
    int vis = m_chart->visibleCount();

    if (!vec.isEmpty()) {
        const KLineData *prev = nullptr;
        if (vec.size() >= 2) prev = &vec[vec.size() - 2];
        m_infoPanel->setData(vec.last(), prev);
    }

    int visStart = m_chart->visibleStart();
    int visEnd = qMin(visStart + vis - 1, total - 1);
    QString totalRange = QStringLiteral("%1 ~ %2")
        .arg(vec.first().date.left(10), vec.last().date.left(10));
    QString visRange;
    if (visStart >= 0 && visStart < total && visEnd >= visStart) {
        visRange = QStringLiteral("%1 ~ %2")
            .arg(vec[visStart].date.left(10), vec[visEnd].date.left(10));
    }

    // 明确提示数据来源与字段缺失，避免用户误以为程序出错
    m_statusLabel->setText(
        QStringLiteral("数据量：%1条（新浪源：不含成交额/换手率） | 可见：%2条 | 区间：%3")
            .arg(total).arg(vis).arg(totalRange));
}

void StockChartWindow::onPeriodChanged(int index)
{
    // 0=分时（真正的分时图）, 1=60分钟, 2=日K, 3=周K, 4=月K
    if (index == 0) {
        switchView(true);
        fetchTimeShare();
        return;
    }

    switchView(false);
    int klts[] = {-1, 60, 101, 102, 103};
    if (index >= 1 && index < 5) {
        m_klt = klts[index];
        fetchKLine();
    }
}

// 切换主视图：K线 / 分时
void StockChartWindow::switchView(bool timeShare)
{
    m_isTimeShare = timeShare;
    if (m_viewStack)
        m_viewStack->setCurrentIndex(timeShare ? 1 : 0);

    // 分时视图下，主图/副图指标下拉没有意义，禁用避免误导
    if (m_mainIndCombo) m_mainIndCombo->setEnabled(!timeShare);
    if (m_subIndCombo)  m_subIndCombo->setEnabled(!timeShare);
}

// ---------- 分时数据（腾讯为主，东财备选）----------
// 说明：东财 trends2 接口盘中偶发 "Connection closed"，而腾讯分时接口
// 稳定性更好，因此以腾讯为主源；东财仅作为备选。
void StockChartWindow::fetchTimeShare()
{
    m_statusLabel->setText(QStringLiteral("分时加载中…"));

    const QString tsCode = m_code.startsWith(QLatin1Char('6'))
                               ? QStringLiteral("sh") + m_code
                               : QStringLiteral("sz") + m_code;

    QUrl url(QStringLiteral("https://web.ifzq.gtimg.cn/appstock/app/minute/query"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("code"), tsCode);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://gu.qq.com/");
    req.setRawHeader("Accept", "application/json, text/plain, */*");
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    QNetworkReply *reply = m_net->get(req);
    const QString capturedCode = tsCode;
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, capturedCode]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(
                QStringLiteral("分时获取失败: %1").arg(reply->errorString()));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject root = doc.object();
        const QJsonObject dataAll = root.value(QStringLiteral("data")).toObject();
        const QJsonObject one = dataAll.value(capturedCode).toObject();

        // 昨收：data.<code>.qt.<code>[4]
        double prevClose = 0;
        QJsonArray qtArr = one.value(QStringLiteral("qt")).toObject()
                              .value(capturedCode).toArray();
        if (qtArr.size() > 4) prevClose = qtArr.at(4).toVariant().toDouble();

        const QJsonObject dataObj = one.value(QStringLiteral("data")).toObject();
        const QString date = dataObj.value(QStringLiteral("date")).toString();
        const QJsonArray arr = dataObj.value(QStringLiteral("data")).toArray();

        if (arr.isEmpty() || prevClose <= 0) {
            m_statusLabel->setText(
                QStringLiteral("分时无数据（可能停牌或非交易日）"));
            return;
        }

        QVector<QString> lines;
        lines.reserve(arr.size());
        for (const QJsonValue &v : arr) lines.append(v.toString());

        // 名称：复用 K 线已获取的名称
        TimeShareData tsd = TimeShare::parseTencent(
            m_code, m_name, prevClose, date, lines);

        if (m_tsChart) m_tsChart->setData(tsd);

        if (!tsd.valid) {
            m_statusLabel->setText(QStringLiteral("分时数据无效"));
            return;
        }

        const auto &last = tsd.last();
        const double pct = TimeShare::chgPct(last.price, tsd.prevClose);
        m_statusLabel->setText(
            QStringLiteral("分时 | %1 %2 | 现价 %3 (%4%5%) | 均价 %6 | 昨收 %7 | %8 点")
                .arg(tsd.date, last.time)
                .arg(last.price, 0, 'f', 2)
                .arg(pct >= 0 ? QStringLiteral("+") : QString())
                .arg(pct, 0, 'f', 2)
                .arg(last.avgPrice, 0, 'f', 3)
                .arg(tsd.prevClose, 0, 'f', 2)
                .arg(tsd.points.size()));
    });

    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
}

void StockChartWindow::onMainIndicatorChanged(int index)
{
    // 0=MA, 1=BOLL, 2=无
    MainIndicatorType types[] = { MainIndicatorType::MA,
                                  MainIndicatorType::BOLL,
                                  MainIndicatorType::None };
    if (index >= 0 && index < 3)
        m_chart->setMainIndicator(types[index]);
}

void StockChartWindow::onSubIndicatorChanged(int index)
{
    // 0=成交量, 1=MACD, 2=KDJ, 3=主力资金
    SubIndicatorType types[] = { SubIndicatorType::Volume,
                                 SubIndicatorType::MACD,
                                 SubIndicatorType::KDJ,
                                 SubIndicatorType::MoneyFlow };
    if (index >= 0 && index < 4)
        m_chart->setSubIndicator(types[index]);
}
