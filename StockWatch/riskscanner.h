#ifndef RISKSCANNER_H
#define RISKSCANNER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QVector>
#include <QStringList>

// ---------- 公告条目 ----------
struct AnnouncementItem {
    QString     title;          // 公告标题
    QString     noticeDate;     // yyyy-MM-dd
    QStringList categories;     // columns[].column_name 列表
};

// ---------- 现金流条目 ----------
struct CashFlowItem {
    QString reportDate;             // 报告期 yyyy-MM-dd
    double  netCashOperate = 0;     // 经营性现金流量净额(元)
    double  totalOperateIncome = 0; // 营业总收入(元, 来自利润表)
    double  incomeYoY = 0;          // 营收同比增长率(%)
    // ---- v3.1 新增（财务质量交叉验证）----
    double  netProfit   = 0;        // 归母净利润(元)
    double  deductNetProfit = 0;    // 扣非归母净利润(元)
    double  reportType = 0;         // 0=未评分；1=年报；2=半年报；3=一季报；4=三季报
    QString auditOpinion;           // 审计意见(仅年报，空=未知)
};

// ---------- 审计意见分类 ----------
enum class AuditOpinionType {
    Unknown,        // 未获取到
    Standard,       // 标准无保留意见
    Emphasis,       // 带强调事项段/持续经营重大不确定性
    Qualified,      // 保留意见
    Adverse,        // 否定意见
    Disclaimer      // 无法表示意见
};

// ---------- 风险结果(纯值类型, 可直接跨线程拷贝) ----------
struct RiskResult {
    QString     code;
    QString     name;
    bool        hasRisk  = false;
    QStringList hits;       // 命中的公告关键词描述(含来源标题/分类)
    QStringList redFlags;   // 财务红旗描述
    QString     summary;    // 一句话汇总
    QStringList dataGaps;   // 数据缺失提示(如审计意见未获取)，提示用户勿把"未知"当"安全"
    bool        auditChecked = false;  // 是否成功获取到审计意见
};

// 财务造假/监管风险扫描器：拉取公告 + 财务现金流，做关键词匹配与财务红旗判断
// 网络请求与解析都在主线程完成，结果通过 riskReady 信号广播给所有订阅者
class RiskScanner : public QObject
{
    Q_OBJECT
public:
    explicit RiskScanner(QNetworkAccessManager *net, QObject *parent = nullptr);

    void scan(const QString &code, const QString &name);             // 单只(在途去重)
    void scanAll(const QStringList &codes,
                 const QMap<QString, QString> &names);               // 批量

    // ---- 纯静态分析(供报告/测试复用, 无网络) ----
    static RiskResult   analyze(const QString &code, const QString &name,
                                const QVector<AnnouncementItem> &anns,
                                const QVector<CashFlowItem> &cfs);
    static QStringList  matchAnnouncementKeywords(const QVector<AnnouncementItem> &anns);
    static QStringList  checkCashFlowRedFlags(const QVector<CashFlowItem> &cfs);
    // 审计意见风险判定（v3.1）：仅「带强调事项」及以上视为红旗
    static QStringList  checkAuditOpinion(const QVector<CashFlowItem> &cfs);
    // 盈利质量判定（v3.1）：扣非净资产收益率/扣非占比/现金流覆盖
    static QStringList  checkEarningsQuality(const QVector<CashFlowItem> &cfs);
    // 审计意见文本归一化
    static AuditOpinionType classifyAuditOpinion(const QString &text);
    // 从公告标题中提取审计意见原文（无则返回空）
    static QString extractAuditOpinion(const QVector<AnnouncementItem> &anns);

signals:
    void riskReady(const QString &code, const RiskResult &result);

private slots:
    void onAnnReply(QNetworkReply *reply);
    void onCfReply(QNetworkReply *reply);
    void onIncomeReply(QNetworkReply *reply);

private:
    static QString secuCode(const QString &code);  // 6开头→.SH, 其余→.SZ
    void checkReady(const QString &code);

    QNetworkAccessManager *m_net;

    struct Pending {
        QString name;
        bool annReady = false, cfReady = false, incomeReady = false;
        QVector<AnnouncementItem> anns;
        QVector<CashFlowItem> cfs;
        QMap<QString, double> incomeByDate;  // reportDate -> 营业总收入
        QMap<QString, double> profitByDate;  // reportDate -> 归母净利润
        QMap<QString, double> deductByDate;  // reportDate -> 扣非归母净利润
        QString auditOpinion;                // 审计意见(空=未知)
    };
    QMap<QString, Pending> m_pending;      // 在途去重
};

#endif // RISKSCANNER_H
