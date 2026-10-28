#ifndef REPORTGENERATOR_H
#define REPORTGENERATOR_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QVector>
#include <QSharedPointer>
#include <QPointer>
#include "klinechart.h"   // KLineData
#include "indicator.h"    // MoneyFlowItem, IndicatorResult
#include "riskscanner.h"  // RiskResult

class RiskScanner;

class ReportGenerator : public QObject
{
    Q_OBJECT
public:
    explicit ReportGenerator(QNetworkAccessManager *net,
                              RiskScanner *scanner,
                              QObject *parent = nullptr);

    /// 为单只股票生成当日报告
    void generate(const QString &code, const QString &name);

    /// 批量生成
    void generateAll(const QStringList &codes,
                     const QMap<QString, QString> &names);

    /// 设置报告输出根目录
    void setReportDir(const QString &dir) { m_reportDir = dir; }

signals:
    void reportReady(const QString &filePath);
    void allDone(int count);
    void error(const QString &msg);

private slots:
    void onKLineReply(QNetworkReply *reply);
    void onMoneyFlowReply(QNetworkReply *reply);
    void onRiskReady(const QString &code, const RiskResult &result);
    void checkAndBuild();
    /// 工作线程完成后在主线程回调
    Q_INVOKABLE void onBuildDone(const QString &path, bool ok);

private:
    // ---- 工具 ----
    QString secId(const QString &code) const;
    QString reportPath(const QString &code) const;
    QString todayStr() const;

    // ---- 格式化 ----
    static QString fmtMoney(double yuan);
    static QString fmtVol(double shares);
    static QString fmtDouble(double v, int decimals = 2);
    static QString chgSign(double v);

    // ---- 报告文本 ----
    static QString buildReportText(const QString &code, const QString &name,
                                   const QVector<KLineData> &klines,
                                   const QVector<MoneyFlowItem> &flows,
                                   const IndicatorResult &r,
                                   const RiskResult &risk);

    // ---- 成员 ----
    QNetworkAccessManager *m_net;
    RiskScanner         *m_scanner;
    QString m_reportDir;

    // 待处理队列: code -> {name, klineDone, flowDone, riskDone}
    struct PendingItem {
        QString name;
        bool klineReady = false;
        bool flowReady = false;
        bool riskReady = false;
        QVector<KLineData> klines;
        QVector<MoneyFlowItem> flows;
        RiskResult risk;
    };
    QMap<QString, PendingItem> m_pending;

    int m_totalCount = 0;
    int m_doneCount  = 0;
    int m_outstandingJobs = 0;  // 还在线程池里跑的作业数

    // 传给工作线程的数据包
    struct BuildData {
        QString code;
        QString name;
        QString baseDir;  // 报告根目录（exe 所在目录）
        QVector<KLineData> klines;
        QVector<MoneyFlowItem> flows;
        RiskResult risk;
    };
};

#endif // REPORTGENERATOR_H
