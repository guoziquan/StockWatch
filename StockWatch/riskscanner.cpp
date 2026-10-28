#include "riskscanner.h"

#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration>
#include <QTimer>
#include <QDateTime>
#include <QtGlobal>
#include <cmath>

// ---------- 常量 ----------
static const char *ANN_API =
    "https://np-anotice-stock.eastmoney.com/api/security/ann";
static const char *CF_API =
    "https://datacenter-web.eastmoney.com/api/data/v1/get";

// ---------- 公告关键词 ----------
// 高危：命中即 hasRisk=true
static const QStringList HIGH_RISK_KEYWORDS = {
    QStringLiteral("立案调查"),
    QStringLiteral("行政处罚"),
    QStringLiteral("事先告知"),
    QStringLiteral("虚假记载"),
    QStringLiteral("财务造假"),
    QStringLiteral("信息披露违法"),
    QStringLiteral("欺诈发行"),
    QStringLiteral("无法表示意见"),
    QStringLiteral("退市")
};
// 关注：命中列入 hits，作为次一级提示
// 注意：这些词必须足够"具体"，否则正常公告会被大面积误报（见下方 isNoise 说明）
static const QStringList WATCH_KEYWORDS = {
    QStringLiteral("风险警示"),
    QStringLiteral("违规担保"),
    QStringLiteral("资金被占用"),
    QStringLiteral("非经营性资金占用"),
    QStringLiteral("保留意见"),
    QStringLiteral("非标意见"),
    QStringLiteral("问询函"),
    QStringLiteral("关注函"),
    QStringLiteral("监管函"),
    QStringLiteral("警示函")
};

// ---------- 分类名噪音 ----------
// 东财公告的 column_name 是"栏目分类"，而非内容判定。
// 例如「资金占用」是每家公司半年报/年报配套的例行汇总表分类
// （《非经营性资金占用及其他关联资金往来情况汇总表》），
// 正常公司也必有此栏目。若直接拿分类名做关键词匹配，会把
// 绝大多数健康公司误判为"存在资金占用风险"。
static const QStringList NOISE_CATEGORIES = {
    QStringLiteral("资金占用"),
    QStringLiteral("专项说明/独立意见"),
    QStringLiteral("保荐/核查意见"),
    QStringLiteral("法律意见书"),
    QStringLiteral("自查报告"),
    QStringLiteral("调研活动"),
    QStringLiteral("董事会决议公告"),
    QStringLiteral("股东大会决议公告"),
    QStringLiteral("股权激励"),
    QStringLiteral("其他")
};

static bool isNoiseCategory(const QString &cat)
{
    for (const QString &n : NOISE_CATEGORIES)
        if (cat.contains(n)) return true;
    return false;
}

// 例行类公告标题特征：这类公告是公司定期披露的"合规证明"，
// 例如《非经营性资金占用及其他关联资金往来情况汇总表》，
// 恰恰是"没有资金占用"的证明文件，绝不能当成风险信号。
static bool isRoutineTitle(const QString &title)
{
    static const QStringList routine = {
        QStringLiteral("汇总表"),
        QStringLiteral("专项说明"),
        QStringLiteral("专项审计说明"),
        QStringLiteral("自查报告"),
        QStringLiteral("履职情况"),
        QStringLiteral("独立财务顾问报告"),
        QStringLiteral("法律意见书"),
        QStringLiteral("会议决议"),
        QStringLiteral("业绩说明会")
    };
    for (const QString &r : routine)
        if (title.contains(r)) return true;
    return false;
}

// ---------- 请求头辅助 ----------
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

// ---------- 构造 ----------
RiskScanner::RiskScanner(QNetworkAccessManager *net, QObject *parent)
    : QObject(parent), m_net(net)
{
    qRegisterMetaType<RiskResult>("RiskResult");
}

QString RiskScanner::secuCode(const QString &code)
{
    return code.startsWith('6') ? code + ".SH" : code + ".SZ";
}

void RiskScanner::scan(const QString &code, const QString &name)
{
    if (code.isEmpty()) return;
    if (m_pending.contains(code)) return;  // 在途去重

    Pending p;
    p.name = name;
    m_pending[code] = p;

    // ---- 公告请求 ----
    QUrl annUrl(ANN_API);
    QUrlQuery q;
    q.addQueryItem("sr", "-1");
    q.addQueryItem("page_size", "20");
    q.addQueryItem("page_index", "1");
    q.addQueryItem("ann_type", "A");
    q.addQueryItem("client_source", "web");
    q.addQueryItem("stock_list", code);
    annUrl.setQuery(q);
    QNetworkReply *annReply = m_net->get(makeReq(annUrl));
    annReply->setProperty("stockCode", code);
    connect(annReply, &QNetworkReply::finished, this,
            [this, annReply]() { onAnnReply(annReply); });
    QTimer::singleShot(15000, annReply, [annReply]() {
        if (annReply->isRunning()) annReply->abort();
    });

    // ---- 财务现金流请求 ----
    QUrl cfUrl(CF_API);
    QUrlQuery fq;
    fq.addQueryItem("reportName", "RPT_DMSK_FN_CASHFLOW");
    fq.addQueryItem("columns", "ALL");
    fq.addQueryItem("filter",
        QStringLiteral("(SECUCODE=\"%1\")").arg(secuCode(code)));
    fq.addQueryItem("pageNumber", "1");
    fq.addQueryItem("pageSize", "12");
    fq.addQueryItem("sortColumns", "REPORT_DATE");
    fq.addQueryItem("sortTypes", "-1");
    fq.addQueryItem("source", "WEB");
    fq.addQueryItem("client", "WEB");
    cfUrl.setQuery(fq);
    QNetworkReply *cfReply = m_net->get(makeReq(cfUrl));
    cfReply->setProperty("stockCode", code);
    connect(cfReply, &QNetworkReply::finished, this,
            [this, cfReply]() { onCfReply(cfReply); });
    QTimer::singleShot(15000, cfReply, [cfReply]() {
        if (cfReply->isRunning()) cfReply->abort();
    });

    // ---- 利润表请求（营收）----
    QUrl incUrl(CF_API);
    QUrlQuery iq;
    iq.addQueryItem("reportName", "RPT_DMSK_FN_INCOME");
    iq.addQueryItem("columns", "ALL");
    iq.addQueryItem("filter",
        QStringLiteral("(SECUCODE=\"%1\")").arg(secuCode(code)));
    iq.addQueryItem("pageNumber", "1");
    iq.addQueryItem("pageSize", "12");
    iq.addQueryItem("sortColumns", "REPORT_DATE");
    iq.addQueryItem("sortTypes", "-1");
    iq.addQueryItem("source", "WEB");
    iq.addQueryItem("client", "WEB");
    incUrl.setQuery(iq);
    QNetworkReply *incReply = m_net->get(makeReq(incUrl));
    incReply->setProperty("stockCode", code);
    connect(incReply, &QNetworkReply::finished, this,
            [this, incReply]() { onIncomeReply(incReply); });
    QTimer::singleShot(15000, incReply, [incReply]() {
        if (incReply->isRunning()) incReply->abort();
    });
}

void RiskScanner::scanAll(const QStringList &codes,
                          const QMap<QString, QString> &names)
{
    for (const QString &code : codes)
        scan(code, names.value(code, code));
}

void RiskScanner::onAnnReply(QNetworkReply *reply)
{
    reply->deleteLater();
    QString code = reply->property("stockCode").toString();
    if (!m_pending.contains(code)) return;

    QVector<AnnouncementItem> anns;
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray list = doc.object().value("data").toObject()
                          .value("list").toArray();
        for (const QJsonValue &v : list) {
            QJsonObject o = v.toObject();
            AnnouncementItem item;
            item.title = o.value("title").toString();
            item.noticeDate = o.value("notice_date").toString().left(10);
            QJsonArray cols = o.value("columns").toArray();
            for (const QJsonValue &cv : cols)
                item.categories << cv.toObject().value("column_name").toString();
            anns.append(item);
        }
    }
    // 失败也标记完成（结果为空，走 analyze 的"无风险"分支）
    m_pending[code].anns = anns;

    // 从公告标题中提取审计意见（东财利润表接口通常不含该字段，
    // 但年报相关公告标题会明确写出审计意见类型，是更可靠的来源）
    m_pending[code].auditOpinion = extractAuditOpinion(anns);

    m_pending[code].annReady = true;
    checkReady(code);
}

void RiskScanner::onCfReply(QNetworkReply *reply)
{
    reply->deleteLater();
    QString code = reply->property("stockCode").toString();
    if (!m_pending.contains(code)) return;

    QVector<CashFlowItem> cfs;
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray arr = doc.object().value("result").toObject()
                         .value("data").toArray();
        for (const QJsonValue &v : arr) {
            QJsonObject o = v.toObject();
            CashFlowItem item;
            item.reportDate = o.value("REPORT_DATE").toString().left(10);
            item.netCashOperate = o.value("NETCASH_OPERATE").toVariant().toDouble();
            cfs.append(item);
        }
    }
    m_pending[code].cfs = cfs;
    m_pending[code].cfReady = true;
    checkReady(code);
}

void RiskScanner::onIncomeReply(QNetworkReply *reply)
{
    reply->deleteLater();
    QString code = reply->property("stockCode").toString();
    if (!m_pending.contains(code)) return;

    QMap<QString, double> incomeByDate;
    QMap<QString, double> profitByDate;
    QMap<QString, double> deductByDate;
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray arr = doc.object().value("result").toObject()
                         .value("data").toArray();
        for (const QJsonValue &v : arr) {
            QJsonObject o = v.toObject();
            const QString date = o.value("REPORT_DATE").toString().left(10);
            incomeByDate[date] = o.value("TOTAL_OPERATE_INCOME").toVariant().toDouble();

            // 归母净利润：东财字段名在不同报表版本下略有差异，依次尝试
            double np = 0;
            const char *npKeys[] = { "PARENT_NETPROFIT", "NETPROFIT",
                                     "NET_PROFIT", "PARENT_NET_PROFIT" };
            for (const char *k : npKeys) {
                const QJsonValue jv = o.value(QLatin1String(k));
                if (!jv.isUndefined() && !jv.isNull()) {
                    np = jv.toVariant().toDouble();
                    break;
                }
            }
            profitByDate[date] = np;

            // 扣非归母净利润
            double dnp = 0;
            const char *dnpKeys[] = { "DEDUCT_PARENT_NETPROFIT", "DEDUCT_PARENT_NET_PROFIT",
                                      "DEDU_PARENT_NETPROFIT", "KCFJCXSYJLR" };
            for (const char *k : dnpKeys) {
                const QJsonValue jv = o.value(QLatin1String(k));
                if (!jv.isUndefined() && !jv.isNull()) {
                    dnp = jv.toVariant().toDouble();
                    break;
                }
            }
            deductByDate[date] = dnp;
        }
    }
    m_pending[code].incomeByDate = incomeByDate;
    m_pending[code].profitByDate = profitByDate;
    m_pending[code].deductByDate = deductByDate;
    m_pending[code].incomeReady = true;
    checkReady(code);
}

void RiskScanner::checkReady(const QString &code)
{
    if (!m_pending.contains(code)) return;
    const Pending &p = m_pending[code];
    if (!p.annReady || !p.cfReady || !p.incomeReady) return;

    Pending item = m_pending.take(code);

    // 把利润表营收/净利润/扣非净利润合并进现金流条目，供财务质量红旗判断
    for (CashFlowItem &cf : item.cfs) {
        cf.totalOperateIncome  = item.incomeByDate.value(cf.reportDate, 0.0);
        cf.netProfit           = item.profitByDate.value(cf.reportDate, 0.0);
        cf.deductNetProfit     = item.deductByDate.value(cf.reportDate, 0.0);
        cf.auditOpinion        = item.auditOpinion;
        // 同比：与上一年同期对比（报告期 yyyy-MM-dd，年份减 1）
        QString prevYear = QString::number(cf.reportDate.left(4).toInt() - 1)
                         + cf.reportDate.mid(4);
        double prevIncome = item.incomeByDate.value(prevYear, 0.0);
        if (prevIncome > 0 && cf.totalOperateIncome > 0)
            cf.incomeYoY = (cf.totalOperateIncome - prevIncome) / prevIncome * 100.0;
    }

    RiskResult result = analyze(code, item.name, item.anns, item.cfs);
    emit riskReady(code, result);
}

// ---------- 纯静态分析 ----------

// =====================================================================
// v3.1 新增：审计意见判定
// =====================================================================
// A 股避雷第一优先级：审计意见是会计师的"背书等级"。
//   * 无法表示意见 / 否定意见  → 最高危，几乎等同财务不可信
//   * 保留意见               → 高危，具体科目存在重大问题
//   * 带强调事项段/持续经营重大不确定性 → 中危，至少说明管理层或经营有问题
//   * 标准无保留意见           → 正常
AuditOpinionType RiskScanner::classifyAuditOpinion(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return AuditOpinionType::Unknown;

    // ---- 第一步：先排除"标准无保留意见" ----
    // 关键陷阱：「标准无保留意见」包含子串「保留意见」，
    // 若先判「保留意见」会把最健康的意见误判成最严重的问题。
    if (t.contains(QStringLiteral("标准无保留")) ||
        t.contains(QStringLiteral("无保留意见"))) {
        // 「带强调事项段的无保留意见」仍属无保留，但需要特别关注
        if (t.contains(QStringLiteral("强调事项")) ||
            t.contains(QStringLiteral("持续经营")) ||
            t.contains(QStringLiteral("带说明段")) ||
            t.contains(QStringLiteral("解释性说明")))
            return AuditOpinionType::Emphasis;
        return AuditOpinionType::Standard;
    }

    // ---- 第二步：按严重程度从高到低判定 ----
    if (t.contains(QStringLiteral("无法表示意见")) ||
        t.contains(QStringLiteral("无法发表意见")))
        return AuditOpinionType::Disclaimer;
    if (t.contains(QStringLiteral("否定意见")))
        return AuditOpinionType::Adverse;
    if (t.contains(QStringLiteral("保留意见")))
        return AuditOpinionType::Qualified;
    if (t.contains(QStringLiteral("强调事项")) ||
        t.contains(QStringLiteral("持续经营")) ||
        t.contains(QStringLiteral("带说明段")) ||
        t.contains(QStringLiteral("解释性说明")))
        return AuditOpinionType::Emphasis;

    return AuditOpinionType::Unknown;
}

QStringList RiskScanner::checkAuditOpinion(const QVector<CashFlowItem> &cfs)
{
    QStringList flags;

    // 只取已带上审计意见的记录中最新的那份年度报告
    for (const auto &cf : cfs) {
        if (cf.auditOpinion.trimmed().isEmpty()) continue;

        switch (classifyAuditOpinion(cf.auditOpinion)) {
        case AuditOpinionType::Disclaimer:
            flags << QStringLiteral("%1 年报被出具「无法表示意见」审计报告，财务数据不可信")
                     .arg(cf.reportDate);
            break;
        case AuditOpinionType::Adverse:
            flags << QStringLiteral("%1 年报被出具「否定意见」审计报告，财务数据严重失真")
                     .arg(cf.reportDate);
            break;
        case AuditOpinionType::Qualified:
            flags << QStringLiteral("%1 年报被出具「保留意见」审计报告")
                     .arg(cf.reportDate);
            break;
        case AuditOpinionType::Emphasis:
            flags << QStringLiteral("%1 年报为「带强调事项段」审计意见，需关注持续经营能力")
                     .arg(cf.reportDate);
            break;
        case AuditOpinionType::Standard:
        case AuditOpinionType::Unknown:
            break;
        }

        // 只评估最新一期年度报告即可
        break;
    }
    return flags;
}

// =====================================================================
// v3.1 新增：盈利质量判定
// =====================================================================
// 核心思想：利润表上的净利润是"算"出来的，现金流是"收"回来的。
// 造假往往体现在：净利润漂亮但现金流跟不上、利润主要靠非经常性损益、
// 以及两者长期背离。
QStringList RiskScanner::checkEarningsQuality(const QVector<CashFlowItem> &cfs)
{
    QStringList flags;
    if (cfs.isEmpty()) return flags;

    auto fmtYi = [](double v) {
        return QString::number(v / 1e8, 'f', 2) + QStringLiteral(" 亿");
    };

    // ---- 1) 扣非净利润占比过低：主业不赚钱，靠卖资产/补贴粉饰 ----
    // 仅在净利润为正时有意义（亏损时占比无意义）
    const auto &latest = cfs[0];
    if (latest.netProfit > 0 && latest.deductNetProfit != 0) {
        const double ratio = latest.deductNetProfit / latest.netProfit;
        if (ratio < 0) {
            flags << QStringLiteral("最新报告期扣非净利润为负(%1)但净利润为正(%2)，利润依赖非经常性损益")
                     .arg(fmtYi(latest.deductNetProfit), fmtYi(latest.netProfit));
        } else if (ratio < 0.5) {
            flags << QStringLiteral("扣非净利润仅占净利润 %1%，主业盈利能力弱")
                     .arg(ratio * 100.0, 0, 'f', 0);
        }
    }

    // ---- 2) 经营现金流 / 净利润 长期背离 ----
    // 取最近 3 期（确保是同一口径的累计数），净利润>0 但现金流合计远低于净利润
    double sumProfit = 0, sumCash = 0;
    int n = 0;
    for (int i = 0; i < cfs.size() && n < 3; ++i) {
        if (cfs[i].netProfit <= 0) continue;   // 亏损期不参与
        sumProfit += cfs[i].netProfit;
        sumCash   += cfs[i].netCashOperate;
        ++n;
    }
    if (n >= 2 && sumProfit > 0) {
        const double cover = sumCash / sumProfit;
        if (cover < 0) {
            flags << QStringLiteral("近%1期累计净利润 %2 但经营现金流净额为 %3，利润未转化为现金")
                     .arg(n).arg(fmtYi(sumProfit), fmtYi(sumCash));
        } else if (cover < 0.5) {
            flags << QStringLiteral("近%1期经营现金流仅覆盖净利润的 %2%，盈利质量存疑")
                     .arg(n).arg(cover * 100.0, 0, 'f', 0);
        }
    }

    return flags;
}

RiskResult RiskScanner::analyze(const QString &code, const QString &name,
                                const QVector<AnnouncementItem> &anns,
                                const QVector<CashFlowItem> &cfs)
{
    RiskResult r;
    r.code = code;
    r.name = name;

    r.hits = matchAnnouncementKeywords(anns);

    // 合并三类财务红旗：审计意见 > 现金流 > 盈利质量（按严重程度排序）
    r.redFlags = checkAuditOpinion(cfs);
    r.redFlags += checkCashFlowRedFlags(cfs);
    r.redFlags += checkEarningsQuality(cfs);

    r.hasRisk = !r.hits.isEmpty() || !r.redFlags.isEmpty();

    // ---- 数据完整性：明确告知哪些维度没能覆盖 ----
    // 审计意见是最重要的否决项，取不到时必须说清楚，
    // 否则用户会把"未报警"误解为"审计没问题"。
    bool hasAuditText = false;
    for (const auto &cf : cfs) {
        if (!cf.auditOpinion.trimmed().isEmpty()) { hasAuditText = true; break; }
    }
    r.auditChecked = hasAuditText;
    if (!hasAuditText) {
        r.dataGaps << QStringLiteral("审计意见未获取（公开接口通常不提供该字段，需查年报原文）");
    }
    if (cfs.isEmpty()) {
        r.dataGaps << QStringLiteral("财务数据未获取");
    } else if (cfs.first().netProfit == 0 && cfs.first().deductNetProfit == 0) {
        r.dataGaps << QStringLiteral("净利润/扣非净利润未获取，盈利质量未评估");
    }

    if (!r.hasRisk) {
        r.summary = QStringLiteral("未发现明显财务造假/监管风险信号");
    } else {
        QStringList parts;
        if (!r.hits.isEmpty())
            parts << QStringLiteral("公告命中「%1」等%2条风险提示")
                     .arg(r.hits.first().section(QStringLiteral("】"), 0, 0)
                             .remove(QStringLiteral("【")))
                     .arg(r.hits.size());
        if (!r.redFlags.isEmpty())
            parts << QStringLiteral("财务%1").arg(r.redFlags.first());
        parts << QStringLiteral("存在财务造假/监管风险，请关注");
        r.summary = parts.join(QStringLiteral("，")) + QStringLiteral("。");
    }
    return r;
}

QString RiskScanner::extractAuditOpinion(const QVector<AnnouncementItem> &anns)
{
    // 现实约束（已用真实接口核实）：
    //   * 东财现金流/利润表接口不含审计意见字段；
    //   * 公告标题通常只写「20XX年度审计报告」，不含意见类型；
    //   * 若出现「（无法表示意见）」这类标题，则会明确写出。
    //
    // 因此这里的策略是：只采纳标题中**明确出现意见类型字样**的公告，
    // 并严格排除「法律意见」「核查意见」「审计委员会」等干扰项。
    // 拿不到时返回空串，由上层显示"审计意见未获取"，
    // 绝不臆测为"标准无保留意见"——把未知当作安全是最危险的。
    static const QStringList noise = {
        QStringLiteral("法律意见"),
        QStringLiteral("核查意见"),
        QStringLiteral("专项意见"),
        QStringLiteral("审计委员会"),
        QStringLiteral("内部控制"),
        QStringLiteral("履职情况")
    };

    for (const auto &ann : anns) {
        const QString t = ann.title;

        // 先排除噪声公告（法律意见书/核查意见/内控审计等）
        bool isNoise = false;
        for (const QString &n : noise) {
            if (t.contains(n)) { isNoise = true; break; }
        }
        if (isNoise) continue;

        // 仅当标题明确写出意见类型时才采纳
        if (classifyAuditOpinion(t) != AuditOpinionType::Unknown)
            return QStringLiteral("%1 %2").arg(ann.noticeDate, t);
    }
    return QString();
}

QStringList RiskScanner::matchAnnouncementKeywords(const QVector<AnnouncementItem> &anns)
{
    QStringList hits;
    for (const auto &ann : anns) {
        QString matched;
        QString level;

        // ---- 0) 例行公告跳过 ----
        // 例行汇总表/自查报告等属于合规证明文件，不是风险信号
        if (isRoutineTitle(ann.title)) continue;

        // ---- 1) 标题匹配（最高置信度）----
        for (const QString &kw : HIGH_RISK_KEYWORDS) {
            if (ann.title.contains(kw)) { matched = kw; level = QStringLiteral("高危"); break; }
        }
        if (matched.isEmpty()) {
            for (const QString &kw : WATCH_KEYWORDS) {
                if (ann.title.contains(kw)) { matched = kw; level = QStringLiteral("关注"); break; }
            }
        }

        // ---- 2) 分类匹配（仅辅助，且必须排除例行分类噪音）----
        if (matched.isEmpty()) {
            for (const QString &cat : ann.categories) {
                if (isNoiseCategory(cat)) continue;      // 例行分类不参与判定
                for (const QString &kw : HIGH_RISK_KEYWORDS) {
                    if (cat.contains(kw)) { matched = kw; level = QStringLiteral("高危"); break; }
                }
                if (!matched.isEmpty()) break;
                for (const QString &kw : WATCH_KEYWORDS) {
                    if (cat.contains(kw)) { matched = kw; level = QStringLiteral("关注"); break; }
                }
                if (!matched.isEmpty()) break;
            }
        }

        if (matched.isEmpty()) continue;

        // 只在"高危"时才值得展示分类信息；关注级别若仅来自分类，可信度低，跳过
        if (level == QStringLiteral("关注") && !ann.title.contains(matched))
            continue;

        QString desc = QStringLiteral("【%1】%2 (%3)")
            .arg(matched, ann.title, ann.noticeDate);
        if (!hits.contains(desc))
            hits << desc;
    }
    return hits;
}

QStringList RiskScanner::checkCashFlowRedFlags(const QVector<CashFlowItem> &cfs)
{
    QStringList flags;
    if (cfs.isEmpty()) return flags;

    // 2) 最新一期大额净流出(< -1亿元)
    const double latestCash = cfs[0].netCashOperate;
    if (latestCash < -1e8)
        flags << QStringLiteral("最新报告期经营性现金流净流出 %1 亿元")
                  .arg(-latestCash / 1e8, 0, 'f', 2);

    // 4) 营收背离 / 5) 营收断崖：单期即可判定
    const double latestIncome = cfs[0].totalOperateIncome;
    const double latestYoY = cfs[0].incomeYoY;
    if (latestIncome > 0 && latestYoY > 20.0 && latestCash < 0)
        flags << QStringLiteral("营收同比增长%1%但经营现金流为负，收入质量存疑")
                  .arg(latestYoY, 0, 'f', 1);
    if (latestIncome > 0 && latestYoY < -50.0)
        flags << QStringLiteral("营收同比下滑 %1%，主业大幅萎缩")
                  .arg(-latestYoY, 0, 'f', 1);

    // ---- 以下判断至少需要 3 期数据 ----
    if (cfs.size() >= 3) {
        // 1) 连续3期经营现金流净额为负
        bool threeNeg = true;
        for (int i = 0; i < 3; ++i) {
            if (cfs[i].netCashOperate >= 0) { threeNeg = false; break; }
        }
        if (threeNeg)
            flags << QStringLiteral("连续3期经营性现金流净额为负");

        // 6) 营收持续萎缩：最近3期同比均下滑，且累计降幅显著
        //    注意：单期小幅下滑是正常经营波动（尤其游戏/周期行业），
        //    不能作为风险信号。仅当"连续下滑 且 最新同比 < -20%"时才提示，
        //    避免把正常行业的版本周期波动误报为风险。
        if (latestYoY < -20.0) {
            int shrink = 0, counted = 0;
            for (int i = 0; i < cfs.size() && counted < 3; ++i) {
                if (std::fabs(cfs[i].incomeYoY) < 1e-9) continue;   // 无同比数据跳过
                if (cfs[i].incomeYoY < 0) ++shrink;
                ++counted;
            }
            if (counted >= 3 && shrink == counted)
                flags << QStringLiteral("近3期营收同比连续下滑(最新%1%)")
                          .arg(latestYoY, 0, 'f', 1);
        }

        // 3) 最近2期逐期下降且最新期为负
        if (cfs[0].netCashOperate < 0 && cfs[0].netCashOperate < cfs[1].netCashOperate)
            flags << QStringLiteral("经营性现金流持续恶化");
    }

    return flags;
}
