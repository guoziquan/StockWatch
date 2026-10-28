#include "reportgenerator.h"
#include "indicator.h"
#include "tradingcalendar.h"

#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDate>
#include <QTime>
#include <QSslConfiguration>
#include <QtConcurrent>
#include <algorithm>

// ========== 常量 ==========
static const char *KLINE_API =
    "https://push2his.eastmoney.com/api/qt/stock/kline/get";
static const char *MONEYFLOW_API =
    "https://push2his.eastmoney.com/api/qt/stock/fflow/daykline/get";

// ========== 构造 ==========

ReportGenerator::ReportGenerator(QNetworkAccessManager *net, RiskScanner *scanner,
                                 QObject *parent)
    : QObject(parent), m_net(net), m_scanner(scanner)
{
    connect(m_scanner, &RiskScanner::riskReady, this, &ReportGenerator::onRiskReady);
}

// ========== 公共接口 ==========

// ========== 请求头辅助 ==========
static QNetworkRequest makeReq(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://quote.eastmoney.com/");
    req.setRawHeader("Accept", "application/json, text/plain, */*");
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    return req;
}

void ReportGenerator::generate(const QString &code, const QString &name)
{
    if (m_pending.contains(code)) return;  // 已经在处理中

    QString path = reportPath(code);
    if (QFile::exists(path)) {
        emit reportReady(path);
        return;  // 今天已生成，跳过
    }

    // 发起 K线请求
    QString sec = secId(code);
    QUrl ku(KLINE_API);
    QUrlQuery kq;
    kq.addQueryItem("secid", sec);
    kq.addQueryItem("klt", "101");        // 日K
    kq.addQueryItem("fqt", "1");          // 前复权
    kq.addQueryItem("fields1", "f1,f2,f3,f4,f5,f6");
    kq.addQueryItem("fields2", "f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61");
    kq.addQueryItem("ut", "b2884a393a59ad64002292a3e90d46a5");
    kq.addQueryItem("lmt", "500");
    kq.addQueryItem("end", "20500101");
    ku.setQuery(kq);
    QNetworkReply *kreply = m_net->get(makeReq(ku));

    // 发起资金流请求
    QUrl fu(MONEYFLOW_API);
    QUrlQuery fq;
    fq.addQueryItem("secid", sec);
    fq.addQueryItem("fields1", "f1,f2,f3,f7");
    fq.addQueryItem("fields2", "f51,f52,f53,f54,f55,f56,f57");
    fq.addQueryItem("lmt", "30");
    fq.addQueryItem("klt", "101");
    fu.setQuery(fq);
    QNetworkReply *freply = m_net->get(makeReq(fu));

    // 注册待处理项
    PendingItem item;
    item.name = name;
    m_pending[code] = item;

    // 触发造假风险扫描（RiskScanner 自带在途去重，与实时扫描并发时只发一次）
    m_scanner->scan(code, name);

    // 连接回调（用 code 标签区分）
    kreply->setProperty("stockCode", code);
    connect(kreply, &QNetworkReply::finished, this, [this, kreply]() {
        onKLineReply(kreply);
    });

    freply->setProperty("stockCode", code);
    connect(freply, &QNetworkReply::finished, this, [this, freply]() {
        onMoneyFlowReply(freply);
    });

    m_totalCount++;
}

void ReportGenerator::generateAll(const QStringList &codes,
                                   const QMap<QString, QString> &names)
{
    if (codes.isEmpty()) return;
    m_totalCount = 0;
    m_doneCount = 0;

    QDir(m_reportDir).mkpath(".");
    for (const QString &code : codes) {
        QString name = names.value(code, code);
        generate(code, name);
    }
}

// ========== 回调 ==========

void ReportGenerator::onKLineReply(QNetworkReply *reply)
{
    reply->deleteLater();
    QString code = reply->property("stockCode").toString();
    if (!m_pending.contains(code)) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit error(QStringLiteral("K线获取失败(%1): %2").arg(code, reply->errorString()));
        m_pending[code].klineReady = true;  // 标记完成（失败）
        checkAndBuild();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray kl = doc.object().value("data").toObject().value("klines").toArray();

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

    m_pending[code].klines = vec;
    m_pending[code].klineReady = true;
    checkAndBuild();
}

void ReportGenerator::onMoneyFlowReply(QNetworkReply *reply)
{
    reply->deleteLater();
    QString code = reply->property("stockCode").toString();
    if (!m_pending.contains(code)) return;

    if (reply->error() != QNetworkReply::NoError) {
        emit error(QStringLiteral("资金流获取失败(%1): %2").arg(code, reply->errorString()));
        m_pending[code].flowReady = true;
        checkAndBuild();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray kl = doc.object().value("data").toObject().value("klines").toArray();

    QVector<MoneyFlowItem> vec;
    vec.reserve(kl.size());
    for (const QJsonValue &v : kl) {
        QStringList parts = v.toString().split(',');
        if (parts.size() < 7) continue;
        MoneyFlowItem m;
        m.date       = parts[0];
        m.mainNet    = parts[1].toDouble();
        m.small      = parts[2].toDouble();
        m.medium     = parts[3].toDouble();
        m.large      = parts[4].toDouble();
        m.superLarge = parts[5].toDouble();
        m.mainRatio  = parts[6].toDouble();
        vec.append(m);
    }

    m_pending[code].flows = vec;
    m_pending[code].flowReady = true;
    checkAndBuild();
}

void ReportGenerator::onRiskReady(const QString &code, const RiskResult &result)
{
    if (!m_pending.contains(code)) return;
    m_pending[code].risk = result;
    m_pending[code].riskReady = true;
    checkAndBuild();
}

// ========== 检查并生成（主线程：查队列 / 工作线程：计算+写文件） ==========

void ReportGenerator::checkAndBuild()
{
    // 找出 K线、资金流、风险都返回了的股票
    QList<QString> readyCodes;
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if (it.value().klineReady && it.value().flowReady && it.value().riskReady)
            readyCodes.append(it.key());
    }

    for (const QString &code : readyCodes) {
        PendingItem item = m_pending.take(code);
        m_doneCount++;

        if (item.klines.size() < 20) {
            emit error(QStringLiteral("%1 K线数据不足").arg(code));
            continue;
        }

        // ===== 计算 + 拼接文本 + 写文件 扔到线程池 =====
        // Qt 5.9 C++11: 不能 lambda capture initializer, 不能 invokeMethod(lambda)
        // 用 QSharedPointer 把数据传进去, 用 QMetaObject::invokeMethod 回调普通槽
        auto data = QSharedPointer<BuildData>::create();
        data->code    = code;
        data->name    = item.name;
        data->baseDir = m_reportDir;
        data->klines  = item.klines;
        data->flows   = item.flows;
        data->risk    = item.risk;

        QPointer<ReportGenerator> self(this);
        m_outstandingJobs++;
        QtConcurrent::run([self, data]() {
            // ---- 工作线程 ----
            IndicatorResult r = IndicatorEngine::compute(data->klines);
            QString text = buildReportText(data->code, data->name,
                                           data->klines, data->flows, r, data->risk);

            // 写文件
            QString path = QStringLiteral("%1/%2/%3.txt")
                .arg(data->baseDir, data->code,
                     QDate::currentDate().toString("yyyy-MM-dd"));
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile f(path);
            bool ok = f.open(QIODevice::WriteOnly | QIODevice::Text);
            if (ok) {
                f.write(text.toUtf8());
                f.close();
            }

            // ---- 回到主线程 ----
            QMetaObject::invokeMethod(self, "onBuildDone",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, path),
                                      Q_ARG(bool, ok));
        });
    }
}

void ReportGenerator::onBuildDone(const QString &path, bool ok)
{
    if (ok)
        emit reportReady(path);
    else
        emit error(QStringLiteral("无法写入报告: %1").arg(path));

    m_outstandingJobs--;
    if (m_outstandingJobs == 0
        && m_doneCount >= m_totalCount
        && m_totalCount > 0) {
        emit allDone(m_doneCount);
        m_totalCount = 0;
        m_doneCount  = 0;
    }
}

// ========== 工具 ==========

QString ReportGenerator::secId(const QString &code) const
{
    return code.startsWith('6') ? "1." + code : "0." + code;
}

QString ReportGenerator::reportPath(const QString &code) const
{
    return QStringLiteral("%1/%2/%3.txt").arg(m_reportDir, code, todayStr());
}

QString ReportGenerator::todayStr() const
{
    return QDate::currentDate().toString("yyyy-MM-dd");
}

// ========== 格式化 ==========

QString ReportGenerator::fmtMoney(double yuan)
{
    if (qAbs(yuan) >= 1e8)
        return QString::number(yuan / 1e8, 'f', 2) + QStringLiteral(" 亿元");
    if (qAbs(yuan) >= 1e4)
        return QString::number(yuan / 1e4, 'f', 0) + QStringLiteral(" 万元");
    return QString::number(yuan, 'f', 0) + QStringLiteral(" 元");
}

QString ReportGenerator::fmtVol(double shares)
{
    if (shares >= 1e8)
        return QString::number(shares / 1e8, 'f', 2) + QStringLiteral(" 亿股");
    if (shares >= 1e4)
        return QString::number(shares / 1e4, 'f', 0) + QStringLiteral(" 万股");
    return QString::number(shares, 'f', 0) + QStringLiteral(" 手");
}

QString ReportGenerator::fmtDouble(double v, int decimals)
{
    if (qAbs(v) < 1e-9) return QStringLiteral("--");
    bool neg = v < 0;
    return (neg ? QStringLiteral("-") : QStringLiteral("+"))
           + QString::number(qAbs(v), 'f', decimals);
}

QString ReportGenerator::chgSign(double v)
{
    if (v > 1e-9) return "+";
    if (v < -1e-9) return "";
    return " ";
}

// ========== 星期 ==========
static QString weekDayName()
{
    int wd = QDate::currentDate().dayOfWeek();  // 1=Mon
    static const char *names[] = {"","一","二","三","四","五","六","日"};
    return QStringLiteral("周%1").arg(names[wd]);
}

// ========== 市场状态 ==========
static QString marketStatus(const QVector<KLineData> &klines)
{
    const QDateTime now = QDateTime::currentDateTime();

    // 用K线最后一条时间判断
    const QString lastDate = klines.last().date;
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");

    // 非交易日(含法定节假日)优先判定
    if (!TradingCalendar::isTradingDay(now.date())) {
        const QString hname = TradingCalendar::holidayName(now.date());
        const QDate nd = TradingCalendar::nextTradingDay(now.date());
        const QString ndStr = nd.isValid()
            ? nd.toString(QStringLiteral("yyyy-MM-dd")) : QStringLiteral("--");
        const QString label = hname.isEmpty() ? QStringLiteral("周末") : hname;
        return QStringLiteral("休市 (%1, 下一交易日 %2)").arg(label, ndStr);
    }

    // 今天是交易日，但K线最后一条不是今天 —— 说明今日数据尚未更新
    if (today != lastDate)
        return QStringLiteral("已收盘 (行情为最近交易日 %1)").arg(lastDate);

    switch (TradingCalendar::phaseAt(now)) {
    case SessionPhase::CallAuction:
    case SessionPhase::CallAuctionEnd:
        return QStringLiteral("集合竞价");
    case SessionPhase::MorningTrading:
    case SessionPhase::AfternoonTrading:
        return QStringLiteral("交易中");
    case SessionPhase::LunchBreak:
        return QStringLiteral("午间休市");
    case SessionPhase::PreOpen:
        return QStringLiteral("未开盘");
    case SessionPhase::AfterClose:
    case SessionPhase::Closed:
        break;
    }
    return QStringLiteral("已收盘");
}

// ========== 报告正文 ==========

QString ReportGenerator::buildReportText(const QString &code, const QString &name,
                                          const QVector<KLineData> &klines,
                                          const QVector<MoneyFlowItem> &flows,
                                          const IndicatorResult &r,
                                          const RiskResult &risk)
{
    const auto &last = klines.last();
    const auto &prev = klines[klines.size() - 2];
    QString mk = marketStatus(klines);
    int flowDays = IndicatorEngine::consecutiveFlowDays(flows);

    QString s;
    s += QStringLiteral(
        "================================================================\n"
        "      %1 (%2)  每日分析报告\n"
        "      日期: %3 (%4)  生成时间: %5\n"
        "================================================================\n\n")
        .arg(name, code)
        .arg(QDate::currentDate().toString("yyyy-MM-dd"))
        .arg(weekDayName())
        .arg(QTime::currentTime().toString("HH:mm:ss"));

    // ------ 市场状态 ------
    s += QStringLiteral("【市场状态】%1\n\n").arg(mk);

    // ------ 行情快照 ------
    s += QStringLiteral("【行情快照】\n");
    s += QStringLiteral("  收盘价: %1 元\n").arg(fmtDouble(last.close));
    s += QStringLiteral("  涨跌幅: %1%\n").arg(fmtDouble(last.chgPct));
    s += QStringLiteral("  今开:   %1     最高:   %2\n")
        .arg(QString::number(last.open, 'f', 2), -10)
        .arg(QString::number(last.high, 'f', 2), -10);
    s += QStringLiteral("  最低:   %1     昨收:   %2\n")
        .arg(QString::number(last.low, 'f', 2), -10)
        .arg(QString::number(prev.close, 'f', 2), -10);
    s += QStringLiteral("\n");

    // ------ 均线 ------
    s += QStringLiteral("【均线系统】\n");
    s += QStringLiteral("  MA5:   %1    MA10:  %2\n")
        .arg(QString::number(r.ma5, 'f', 2), -9)
        .arg(QString::number(r.ma10, 'f', 2));
    s += QStringLiteral("  MA20:  %1    MA60:  %2\n")
        .arg(QString::number(r.ma20, 'f', 2), -9)
        .arg(r.ma60 > 0 ? QString::number(r.ma60, 'f', 2) : QStringLiteral("--"));
    s += QStringLiteral("  均线排列: %1\n\n").arg(r.maArrangement);

    // ------ MACD ------
    s += QStringLiteral("【MACD】\n");
    s += QStringLiteral("  DIF:    %1    DEA: %2    MACD柱: %3\n")
        .arg(QString::number(r.dif, 'f', 2), -9)
        .arg(QString::number(r.dea, 'f', 2), -9)
        .arg(QString::number(r.macdBar, 'f', 2));
    if (r.deathCross)
        s += QStringLiteral("  信号:  死叉 —— 短期动能减弱，注意风险\n");
    else if (r.goldenCross)
        s += QStringLiteral("  信号:  金叉 —— 短期动能增强，关注机会\n");
    else if (r.dif > r.dea)
        s += QStringLiteral("  信号:  多头运行\n");
    else
        s += QStringLiteral("  信号:  空头运行\n");
    s += QStringLiteral("\n");

    // ------ KDJ ------
    s += QStringLiteral("【KDJ】\n");
    s += QStringLiteral("  K: %1    D: %2    J: %3\n")
        .arg(QString::number(r.k, 'f', 2))
        .arg(QString::number(r.d, 'f', 2))
        .arg(QString::number(r.j, 'f', 2));
    QString kdjStatus;
    if (r.kdjOverbought)        kdjStatus = QStringLiteral("超买 —— 短期过热，注意回调");
    else if (r.kdjOversold)     kdjStatus = QStringLiteral("超卖 —— 超跌区域，关注反弹");
    else if (r.k > 50)          kdjStatus = QStringLiteral("偏强");
    else                        kdjStatus = QStringLiteral("偏弱");
    s += QStringLiteral("  状态: %1\n\n").arg(kdjStatus);

    // ------ RSI ------
    s += QStringLiteral("【RSI (14日)】\n");
    s += QStringLiteral("  RSI: %1\n").arg(QString::number(r.rsi14, 'f', 2));
    QString rsiStatus;
    if (r.rsiOverbought)        rsiStatus = QStringLiteral("超买");
    else if (r.rsiOversold)     rsiStatus = QStringLiteral("超卖");
    else if (r.rsi14 >= 50)     rsiStatus = QStringLiteral("偏强");
    else                        rsiStatus = QStringLiteral("中性偏弱");
    s += QStringLiteral("  状态: %1\n\n").arg(rsiStatus);

    // ------ 布林带 ------
    s += QStringLiteral("【布林带 (20,2)】\n");
    s += QStringLiteral("  上轨: %1    中轨: %2    下轨: %3\n")
        .arg(QString::number(r.bollUp, 'f', 2))
        .arg(QString::number(r.bollMid, 'f', 2))
        .arg(QString::number(r.bollDown, 'f', 2));
    s += QStringLiteral("  带宽: %1%\n").arg(QString::number(r.bollBandwidth, 'f', 2));
    if (last.close > r.bollMid)
        s += QStringLiteral("  当前: 股价运行在中轨与上轨之间\n");
    else
        s += QStringLiteral("  当前: 股价运行在中轨与下轨之间\n");
    s += QStringLiteral("\n");

    // ------ 量价分析 ------
    s += QStringLiteral("【量价分析】\n");
    s += QStringLiteral("  今日成交量: %1 (成交额 %2)\n")
        .arg(fmtVol(last.volume))
        .arg(fmtMoney(last.amount));
    s += QStringLiteral("  5日均量:    %1\n").arg(fmtVol(r.volMa5));
    s += QStringLiteral("  量比:       %1\n").arg(QString::number(r.volRatio, 'f', 2));
    s += QStringLiteral("  量价关系:   %1\n\n").arg(r.volumeDesc);

    // ------ 资金面 ------
    s += QStringLiteral("【资金面】\n");
    if (!flows.isEmpty()) {
        const auto &flast = flows.last();
        s += QStringLiteral("  主力净流入:  %1\n").arg(fmtMoney(flast.mainNet));
        s += QStringLiteral("  主力净占比:  %1%\n").arg(fmtDouble(flast.mainRatio));
        s += QStringLiteral("  超大单:      %1\n").arg(fmtMoney(flast.superLarge));
        s += QStringLiteral("  大单:        %1\n").arg(fmtMoney(flast.large));
        s += QStringLiteral("  中单:        %1\n").arg(fmtMoney(flast.medium));
        s += QStringLiteral("  小单:        %1\n").arg(fmtMoney(flast.small));

        if (flowDays > 1)
            s += QStringLiteral("  趋势: 连续%1日净流入\n").arg(flowDays);
        else if (flowDays < -1)
            s += QStringLiteral("  趋势: 连续%1日净流出\n").arg(-flowDays);
        else if (flowDays == 1)
            s += QStringLiteral("  趋势: 今日净流入\n");
        else if (flowDays == -1)
            s += QStringLiteral("  趋势: 今日净流出\n");
        else
            s += QStringLiteral("  趋势: 无显著方向\n");
    } else {
        s += QStringLiteral("  (无资金流数据)\n");
    }
    s += QStringLiteral("\n");

    // ------ 支撑与阻力 ------
    s += QStringLiteral("【支撑与阻力】\n");
    if (r.support > 0)
        s += QStringLiteral("  支撑位: %1 元  (%2)\n")
            .arg(QString::number(r.support, 'f', 2), r.supportSrc);
    else
        s += QStringLiteral("  支撑位: --\n");
    if (r.resistance < 999999)
        s += QStringLiteral("  阻力位: %1 元  (%2)\n")
            .arg(QString::number(r.resistance, 'f', 2), r.resistanceSrc);
    else
        s += QStringLiteral("  阻力位: --\n");
    s += QStringLiteral("\n");

    // ------ 风险提示 ------
    s += QStringLiteral("【风险提示】\n");
    if (r.warnings.isEmpty()) {
        s += QStringLiteral("  暂无显著风险信号\n");
    } else {
        for (const QString &w : r.warnings)
            s += QStringLiteral("  ⚠ %1\n").arg(w);
    }
    s += QStringLiteral("\n");

    // ------ 造假风险 ------
    s += QStringLiteral("【造假风险】\n");
    if (!risk.hasRisk) {
        s += QStringLiteral("  未发现明显的财务造假/监管风险信号\n");
    } else {
        for (const QString &h : risk.hits)
            s += QStringLiteral("  ⚠ 公告: %1\n").arg(h);
        for (const QString &f : risk.redFlags)
            s += QStringLiteral("  ⚠ 财务: %1\n").arg(f);
        s += QStringLiteral("  汇总: %1\n").arg(risk.summary);
    }
    s += QStringLiteral("\n");

    // ------ 综合研判 ------
    s += QStringLiteral("【综合研判】\n  %1\n\n").arg(r.summary);

    // ------ 脚注 ------
    s += QStringLiteral(
        "================================================================\n"
        "  报告由 StockWatch 自动生成 · 仅供参考，不构成投资建议\n"
        "================================================================\n");

    return s;
}
