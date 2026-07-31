#include "klinedialog.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

static const char *KLINE_API =
    "https://push2his.eastmoney.com/api/qt/stock/kline/get";

KLineDialog::KLineDialog(const QString &code, const QString &name,
                         QWidget *parent)
    : QDialog(parent)
    , m_code(code)
    , m_name(name)
    , m_net(new QNetworkAccessManager(this))
{
    setWindowTitle(QStringLiteral("%1(%2) - K线图").arg(name, code));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(1000, 680);
    setMinimumSize(700, 450);

    // 深色背景
    setStyleSheet(QStringLiteral("background:#0d1422;color:#e8eefc;"));

    buildUi();
    fetchKLine();  // 默认加载日K
}

void KLineDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 6);
    root->setSpacing(6);

    // ---- 顶部栏 ----
    auto *top = new QHBoxLayout;
    m_titleLabel = new QLabel(QStringLiteral("%1 (%2)").arg(m_name, m_code), this);
    m_titleLabel->setStyleSheet("font-size:16px;font-weight:bold;color:#4da3ff;");

    m_periodCombo = new QComboBox(this);
    m_periodCombo->addItems({QStringLiteral("5分钟"), QStringLiteral("60分钟"),
                              QStringLiteral("日K"), QStringLiteral("周K"),
                              QStringLiteral("月K")});
    m_periodCombo->setCurrentIndex(2);  // 默认日K
    m_periodCombo->setStyleSheet(
        "QComboBox{background:#131f33;border:1px solid #22334d;border-radius:6px;"
        "padding:5px 10px;color:white;min-width:90px;}");
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

    top->addWidget(m_titleLabel);
    top->addStretch();
    top->addWidget(m_periodCombo);
    top->addWidget(refreshBtn);
    root->addLayout(top);

    // ---- 图表 ----
    m_chart = new KLineChart(this);
    root->addWidget(m_chart, 1);

    // ---- 底部状态 ----
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
    q.addQueryItem("fields2", "f51,f52,f53,f54,f55,f56,f57");
    q.addQueryItem("ut", "7eea3edcaed734bea9cbfc24409ed989");
    // 不同周期请求不同条数
    int lmt = (m_klt == 5 || m_klt == 60) ? 300 : 500;
    q.addQueryItem("lmt", QString::number(lmt));
    q.addQueryItem("end", "20500101");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    req.setRawHeader("Referer", "https://data.eastmoney.com/");

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onReplyFinished(reply); });
}

void KLineDialog::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText(QStringLiteral("请求失败: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject dataObj = doc.object().value("data").toObject();
    QJsonArray kl = dataObj.value("klines").toArray();
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
        vec.append(d);
    }

    m_chart->setData(vec);
    int total = vec.size();
    m_statusLabel->setText(
        QStringLiteral("数据量: %1 条 · %2 ~ %3")
            .arg(total)
            .arg(vec.first().date)
            .arg(vec.last().date));
}
