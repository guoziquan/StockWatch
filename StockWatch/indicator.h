#ifndef INDICATOR_H
#define INDICATOR_H

#include <QString>
#include <QStringList>
#include <QVector>
#include "klinechart.h"  // KLineData

// ---------- 资金流向条目 ----------
struct MoneyFlowItem
{
    QString date;
    double mainNet = 0;      // 主力净流入(元)
    double mainRatio = 0;    // 主力净占比(%)
    double superLarge = 0;   // 超大单净流入
    double large = 0;        // 大单净流入
    double medium = 0;       // 中单净流入
    double small = 0;        // 小单净流入
};

// ---------- 技术指标计算结果 ----------
struct IndicatorResult
{
    // 均线
    double ma5  = 0, ma10 = 0, ma20 = 0, ma60 = 0;
    // MACD
    double dif  = 0, dea = 0, macdBar = 0;
    bool   goldenCross = false;   // 近3日金叉
    bool   deathCross  = false;   // 近3日死叉
    // KDJ
    double k = 0, d = 0, j = 0;
    bool   kdjOverbought = false;  // K>80
    bool   kdjOversold   = false;  // K<20
    // RSI
    double rsi14 = 0;
    bool   rsiOverbought = false;  // >70
    bool   rsiOversold   = false;  // <30
    // 布林带
    double bollMid = 0, bollUp = 0, bollDown = 0;
    double bollBandwidth = 0;      // 带宽%
    // 量价
    double volMa5 = 0;
    double volRatio = 0;
    QString volumeDesc;            // "放量上涨"/"缩量下跌"/"量价正常" 等
    // 支撑阻力
    double support = 0, resistance = 0;
    QString supportSrc, resistanceSrc;
    // 均线排列
    QString maArrangement;         // "多头排列"/"空头排列"/"交叉整理"/"混乱"
    // 风险提示
    QStringList warnings;
    // 一句话总结
    QString summary;
};

// ---------- 技术指标计算引擎 ----------
class IndicatorEngine
{
public:
    /// 从日K线数据计算全部指标（使用最新一根K线的位置）
    static IndicatorResult compute(const QVector<KLineData> &data);

    /// 计算全序列 BOLL (20,2)
    static void bollSeries(const QVector<KLineData> &d,
                           QVector<double> &up, QVector<double> &mid, QVector<double> &down);

    /// 计算全序列 MACD (12,26,9)
    static void macdSeries(const QVector<KLineData> &d,
                           QVector<double> &dif, QVector<double> &dea, QVector<double> &bar);

    /// 计算全序列 KDJ (9,3,3)
    static void kdjSeries(const QVector<KLineData> &d,
                          QVector<double> &k, QVector<double> &dval, QVector<double> &j);

    /// 分析资金流向连续趋势
    static int consecutiveFlowDays(const QVector<MoneyFlowItem> &flows);

private:
    // ---- 各指标 ----
    static void calcMA(const QVector<KLineData> &d, IndicatorResult &r);
    static void calcMACD(const QVector<KLineData> &d, IndicatorResult &r);
    static void calcKDJ(const QVector<KLineData> &d, IndicatorResult &r);
    static void calcRSI(const QVector<KLineData> &d, IndicatorResult &r);
    static void calcBollinger(const QVector<KLineData> &d, IndicatorResult &r);
    static void calcVolume(const QVector<KLineData> &d, IndicatorResult &r);
    static void calcSupport(const QVector<KLineData> &d, IndicatorResult &r);

    // ---- 综合判定 ----
    static void checkRisks(const QVector<KLineData> &d, IndicatorResult &r);
    static void buildSummary(const QVector<KLineData> &d, IndicatorResult &r);

    // ---- EMA 辅助 ----
    static double ema(double prevEma, double currentPrice, int period);
    static QVector<double> emaSeries(const QVector<double> &prices, int period);
};

#endif // INDICATOR_H
