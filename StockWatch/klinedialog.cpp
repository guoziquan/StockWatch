#include "klinedialog.h"

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

static const char *KLINE_API =
    "https://push2his.eastmoney.com/api/qt/stock/kline/get";

KLineDialog::KLineDialog(const QString &code, const QString &name,
                         QNetworkAccessManager *sharedNet,
                         QWidget *parent)
    : QDialog(parent)
    , m_code(code)
    , m_name(name)
    , m_net(sharedNet)
{
    setWindowTitle(QStringLiteral("%1(%2) - K线图").arg(name, code));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(1100, 680);
    setMinimumSize(800, 450);

    // 深色背景
    setStyleSheet(QStringLiteral("background:#0d1422;color:#e8eefc;"));

    buildUi();
    fetchKLine();
}

void KLineDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 4);
    root->setSpacing(4);

    // ---- 顶部工具栏 ----
    auto *topBar = new QHBoxLayout;
    m_titleLabel = new QLabel(QStringLiteral("%1 (%2)").arg(m_name, m_code), this);
    m_titleLabel->setStyleSheet("font-size:15px;font-weight:bold;color:#4da3ff;");

    m_periodCombo = new QComboBox(this);
    m_periodCombo->addItems({QStringLiteral("5分钟"), QStringLiteral("60分钟"),
                              QStringLiteral("日K"), QStringLiteral("周K"),
                              QStringLiteral("月K")});
    m_periodCombo->setCurrentIndex(2);
    m_periodCombo->setStyleSheet(
        "QComboBox{background:#131f33;border:1px solid #22334d;border-radius:6px;"
        "padding:5px 10px;color:white;min-width:80px;}");
    connect(m_periodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        int klts[] = {5, 60, 101, 102, 103};
        m_klt = klts[idx];
        fetchKLine();
    });

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    refreshBtn->setStyleSheet(
        "QPushButton{background:#131f33;color:white;border:1px solid #22334d;"
        "border-radius:6px;padding:5px 14px;}"
        "QPushButton:hover{background:#182742;}");
    connect(refreshBtn, &QPushButton::clicked, this, &KLineDialog::fetchKLine);

    topBar->addWidget(m_titleLabel);
    topBar->addStretch();
    topBar->addWidget(m_periodCombo);
    topBar->addWidget(refreshBtn);
    root->addLayout(topBar);

    // ---- 中间主体：左图表 + 右信息面板 ----
    auto *body = new QHBoxLayout;
    body->setSpacing(6);

    // 左侧图表
    m_chart = new KLineChart(this);
    connect(m_chart, &KLineChart::hoveredChanged, this,
            [this](const KLineData &kd) {
        // 悬停更新信息面板，取前一日的 close 当昨收
        int idx = -1;
        for (int i = 0; i < (int)m_cacheData.size(); ++i) {
            if (m_cacheData[i].date == kd.date) { idx = i; break; }
        }
        const KLineData *prev = nullptr;
        if (idx > 0 && idx < m_cacheData.size())
            prev = &m_cacheData[idx - 1];
        m_infoPanel->setData(kd, prev);
    });
    body->addWidget(m_chart, 1);

    // 右侧信息面板
    m_infoPanel = new KLineInfoPanel(this);
    body->addWidget(m_infoPanel);

    root->addLayout(body, 1);

    // ---- 底部状态栏 ----
    m_statusLabel = new QLabel(QStringLiteral("加载中…"), this);
    m_statusLabel->setStyleSheet("color:#8fa3c0;font-size:11px;");
    root->addWidget(m_statusLabel);
}

QString KLineDialog::secId() const
{
    return m_code.startsWith('6') ? "1." + m_code : "0." + m_code;
}

void KLineDialog::fetchKLine()
{
    m_statusLabel->setText(QStringLiteral("加载中…"));

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

void KLineDialog::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText(QStringLiteral("请求失败: %1 (code=%2)")
                               .arg(reply->errorString()).arg(reply->error()));
        return;
    }

    QByteArray body = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonArray kl = doc.object().value("data").toObject().value("klines").toArray();
    if (kl.isEmpty()) {
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

    int total = vec.size();
    int vis = m_chart->visibleCount();

    // 初始显示最新数据，更新信息面板
    if (!vec.isEmpty()) {
        const KLineData *prev = nullptr;
        if (vec.size() >= 2) prev = &vec[vec.size() - 2];
        m_infoPanel->setData(vec.last(), prev);
    }

    // 状态栏：总量 + 可见量 + 总区间 + 可见区间
    QString totalRange = QStringLiteral("%1 ~ %2")
        .arg(vec.first().date.left(10), vec.last().date.left(10));
    QString visRange;
    int visStart = m_chart->visibleStart();
    int visEnd = qMin(visStart + vis - 1, total - 1);
    if (visStart >= 0 && visStart < total && visEnd >= visStart) {
        visRange = QStringLiteral("%1 ~ %2")
            .arg(vec[visStart].date.left(10), vec[visEnd].date.left(10));
    }

    m_statusLabel->setText(
        QStringLiteral("数据量：%1条 | 当前可见：%2条 | 总区间：%3 | 可见区间：%4")
            .arg(total).arg(vis).arg(totalRange, visRange));
}
