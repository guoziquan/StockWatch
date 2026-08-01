#include "indicator.h"
#include <QtMath>
#include <QDate>
#include <algorithm>

// ========== 公共接口 ==========

IndicatorResult IndicatorEngine::compute(const QVector<KLineData> &data)
{
    IndicatorResult r;
    if (data.size() < 20) return r;  // 至少需要20根K线

    calcMA(data, r);
    calcMACD(data, r);
    calcKDJ(data, r);
    calcRSI(data, r);
    calcBollinger(data, r);
    calcVolume(data, r);
    calcSupport(data, r);
    checkRisks(data, r);
    buildSummary(data, r);
    return r;
}

int IndicatorEngine::consecutiveFlowDays(const QVector<MoneyFlowItem> &flows)
{
    if (flows.isEmpty()) return 0;
    int count = 0;
    bool positive = flows.last().mainNet > 0;
    for (int i = flows.size() - 1; i >= 0; --i) {
        bool curPos = flows[i].mainNet > 0;
        if (curPos == positive)
            count++;
        else
            break;
    }
    return positive ? count : -count;  // 正=连续流入, 负=连续流出
}

// ========== EMA 辅助 ==========

double IndicatorEngine::ema(double prevEma, double currentPrice, int period)
{
    double k = 2.0 / (period + 1.0);
    return currentPrice * k + prevEma * (1.0 - k);
}

QVector<double> IndicatorEngine::emaSeries(const QVector<double> &prices, int period)
{
    QVector<double> result(prices.size(), 0);
    if (prices.isEmpty()) return result;

    // 首日 EMA = 首日收盘价
    result[0] = prices[0];
    for (int i = 1; i < prices.size(); ++i)
        result[i] = ema(result[i - 1], prices[i], period);
    return result;
}

// ========== 均线 MA ==========

void IndicatorEngine::calcMA(const QVector<KLineData> &d, IndicatorResult &r)
{
    int n = d.size();
    if (n < 5) return;

    auto avg = [&](int cnt) -> double {
        if (n < cnt) return 0;
        double sum = 0;
        for (int i = n - cnt; i < n; ++i) sum += d[i].close;
        return sum / cnt;
    };

    r.ma5  = avg(5);
    r.ma10 = avg(10);
    r.ma20 = avg(20);
    r.ma60 = n >= 60 ? avg(60) : 0;

    // 均线排列判定
    double ma5v = r.ma5, ma10v = r.ma10, ma20v = r.ma20, ma60v = r.ma60;
    if (ma60v > 0) {
        if (ma5v > ma10v && ma10v > ma20v && ma20v > ma60v)
            r.maArrangement = QStringLiteral("多头排列");
        else if (ma5v < ma10v && ma10v < ma20v && ma20v < ma60v)
            r.maArrangement = QStringLiteral("空头排列");
        else if ((d.last().close > ma5v && d.last().close < ma20v) ||
                 (d.last().close < ma5v && d.last().close > ma20v))
            r.maArrangement = QStringLiteral("交叉整理");
        else
            r.maArrangement = QStringLiteral("混乱");
    } else {
        if (ma5v > ma10v && ma10v > ma20v)
            r.maArrangement = QStringLiteral("短多排列");
        else if (ma5v < ma10v && ma10v < ma20v)
            r.maArrangement = QStringLiteral("短空排列");
        else
            r.maArrangement = QStringLiteral("交叉整理");
    }
}

// ========== MACD (12,26,9) ==========

void IndicatorEngine::calcMACD(const QVector<KLineData> &d, IndicatorResult &r)
{
    int n = d.size();
    if (n < 33) return;  // EMA26 需要足够数据

    // 构建收盘价序列
    QVector<double> closes;
    closes.reserve(n);
    for (const auto &k : d) closes.append(k.close);

    // EMA12 和 EMA26
    // 初始值用 SMA
    double init12 = 0, init26 = 0;
    for (int i = 0; i < n; ++i) {
        if (i < 12) init12 += closes[i]; else break;
    }
    init12 /= 12;
    for (int i = 0; i < 26; ++i) init26 += closes[i];
    init26 /= 26;

    QVector<double> dif(n), dea(n), macd(n);
    double ema12 = init12, ema26 = init26;

    for (int i = 0; i < n; ++i) {
        ema12 = ema(ema12, closes[i], 12);
        ema26 = ema(ema26, closes[i], 26);
        dif[i] = ema12 - ema26;
    }

    // DEA = EMA9 of DIF
    double ema9 = dif[0];
    dea[0] = ema9;
    for (int i = 1; i < n; ++i) {
        ema9 = ema(ema9, dif[i], 9);
        dea[i] = ema9;
        macd[i] = 2.0 * (dif[i] - dea[i]);
    }

    r.dif  = dif.last();
    r.dea  = dea.last();
    r.macdBar = macd.last();

    // 金叉/死叉检测（最近3日）
    for (int i = qMax(0, n - 3); i < n - 1; ++i) {
        bool prevAbove = dif[i] > dea[i];
        bool curAbove  = dif[i + 1] > dea[i + 1];
        if (!prevAbove && curAbove)  r.goldenCross = true;
        if (prevAbove && !curAbove)  r.deathCross = true;
    }
}

// ========== KDJ (9,3,3) ==========

void IndicatorEngine::calcKDJ(const QVector<KLineData> &d, IndicatorResult &r)
{
    int n = d.size();
    if (n < 9) return;

    double k = 50, dv = 50;  // 初始 K=D=50
    QVector<double> K(n), D(n), J(n);

    for (int i = 8; i < n; ++i) {
        // 最低价LLV(9) 和最高价HHV(9)
        double low9 = d[i].low, high9 = d[i].high;
        for (int t = i - 8; t <= i; ++t) {
            if (d[t].low  < low9)  low9  = d[t].low;
            if (d[t].high > high9) high9 = d[t].high;
        }
        double range = high9 - low9;
        double rsv = (range < 1e-9) ? 50 : 100.0 * (d[i].close - low9) / range;

        k = 2.0 / 3.0 * k + 1.0 / 3.0 * rsv;
        dv = 2.0 / 3.0 * dv + 1.0 / 3.0 * k;
        K[i] = k;
        D[i] = dv;
        J[i] = 3.0 * k - 2.0 * dv;
    }

    r.k = K.last();
    r.d = D.last();
    r.j = J.last();
    r.kdjOverbought = (r.k > 80);
    r.kdjOversold   = (r.k < 20);
}

// ========== RSI (14) Wilder平滑 ==========

void IndicatorEngine::calcRSI(const QVector<KLineData> &d, IndicatorResult &r)
{
    int n = d.size();
    if (n < 15) return;

    double avgGain = 0, avgLoss = 0;

    // 前14天用于初始化
    for (int i = n - 14; i < n; ++i) {
        double chg = d[i].close - d[i - 1].close;
        if (chg > 0) avgGain += chg;
        else         avgLoss += (-chg);
    }
    avgGain /= 14;
    avgLoss /= 14;

    // Wilder 平滑
    for (int i = n - 13; i < n; ++i) {
        double chg = d[i].close - d[i - 1].close;
        double gain = (chg > 0) ? chg : 0;
        double loss = (chg < 0) ? (-chg) : 0;
        avgGain = (avgGain * 13 + gain) / 14;
        avgLoss = (avgLoss * 13 + loss) / 14;
    }

    double rs = (avgLoss < 1e-9) ? 100 : avgGain / avgLoss;
    r.rsi14 = 100.0 - 100.0 / (1.0 + rs);
    r.rsiOverbought = (r.rsi14 > 70);
    r.rsiOversold   = (r.rsi14 < 30);
}

// ========== 布林带 (20,2) ==========

void IndicatorEngine::calcBollinger(const QVector<KLineData> &d, IndicatorResult &r)
{
    int n = d.size();
    if (n < 20) return;

    // MA20 (最近20日)
    double sum = 0;
    for (int i = n - 20; i < n; ++i) sum += d[i].close;
    double mid = sum / 20;

    // 样本标准差 (n-1)
    double sumSq = 0;
    for (int i = n - 20; i < n; ++i) {
        double diff = d[i].close - mid;
        sumSq += diff * diff;
    }
    double stddev = qSqrt(sumSq / 19);  // 样本标准差 n-1

    r.bollMid = mid;
    r.bollUp  = mid + 2.0 * stddev;
    r.bollDown = mid - 2.0 * stddev;
    r.bollBandwidth = (mid > 0) ? (r.bollUp - r.bollDown) / mid * 100 : 0;
}

// ========== 量价分析 ==========

void IndicatorEngine::calcVolume(const QVector<KLineData> &d, IndicatorResult &r)
{
    int n = d.size();
    if (n < 5) return;

    double sum = 0;
    for (int i = n - 5; i < n; ++i) sum += d[i].volume;
    r.volMa5 = sum / 5;
    r.volRatio = (r.volMa5 > 0) ? d.last().volume / r.volMa5 : 1;

    double chg = d.last().chgPct;
    if (r.volRatio > 1.3) {
        if (chg > 1)       r.volumeDesc = QStringLiteral("放量上涨 —— 多头力量强劲");
        else if (chg < -1) r.volumeDesc = QStringLiteral("放量下跌 —— 空头力量强劲，注意风险");
        else               r.volumeDesc = QStringLiteral("放量滞涨 —— 多空分歧加大，关注方向选择");
    } else if (r.volRatio < 0.7) {
        if (chg > 0)       r.volumeDesc = QStringLiteral("缩量上涨 —— 上涨动能不足，关注持续性");
        else if (chg < 0)  r.volumeDesc = QStringLiteral("缩量下跌 —— 下跌动能减弱，关注企稳信号");
        else               r.volumeDesc = QStringLiteral("缩量横盘 —— 交投清淡，等待方向选择");
    } else {
        if (chg > 1)       r.volumeDesc = QStringLiteral("温和放量上涨 —— 趋势健康");
        else if (chg < -1) r.volumeDesc = QStringLiteral("温和放量下跌 —— 空方占优");
        else               r.volumeDesc = QStringLiteral("量价正常");
    }
}

// ========== 支撑与阻力 ==========

void IndicatorEngine::calcSupport(const QVector<KLineData> &d, IndicatorResult &r)
{
    int n = d.size();
    if (n < 20) return;

    double price = d.last().close;

    // 候选支撑（价格下方的最近MA/布林下轨/20日最低）
    struct Candidate { double level; QString src; };
    QVector<Candidate> supports, resistances;

    if (r.ma5 > 0 && r.ma5 < price)     supports.append({r.ma5, QStringLiteral("MA5")});
    if (r.ma10 > 0 && r.ma10 < price)   supports.append({r.ma10, QStringLiteral("MA10")});
    if (r.ma20 > 0 && r.ma20 < price)   supports.append({r.ma20, QStringLiteral("MA20")});
    if (r.ma60 > 0 && r.ma60 < price)   supports.append({r.ma60, QStringLiteral("MA60")});
    if (r.bollDown > 0 && r.bollDown < price) supports.append({r.bollDown, QStringLiteral("布林下轨")});

    if (r.ma5 > price)     resistances.append({r.ma5, QStringLiteral("MA5")});
    if (r.ma10 > price)    resistances.append({r.ma10, QStringLiteral("MA10")});
    if (r.ma20 > price)    resistances.append({r.ma20, QStringLiteral("MA20")});
    if (r.ma60 > price)    resistances.append({r.ma60, QStringLiteral("MA60")});
    if (r.bollUp > price)  resistances.append({r.bollUp, QStringLiteral("布林上轨")});

    // 20日高低点
    double low20 = d.last().low, high20 = d.last().high;
    for (int i = n - 20; i < n; ++i) {
        if (d[i].low < low20)   low20 = d[i].low;
        if (d[i].high > high20) high20 = d[i].high;
    }
    if (low20 < price)    supports.append({low20, QStringLiteral("20日低点")});
    if (high20 > price)   resistances.append({high20, QStringLiteral("20日高点")});

    // 找最近支撑（价格下方最远的那个 = 最接近的）
    double bestSup = 0, bestRes = 999999;
    for (const auto &c : supports)
        if (c.level > bestSup) { bestSup = c.level; r.supportSrc = c.src; }
    for (const auto &c : resistances)
        if (c.level < bestRes) { bestRes = c.level; r.resistanceSrc = c.src; }

    r.support = bestSup;
    r.resistance = bestRes;
}

// ========== 风险检测 ==========

void IndicatorEngine::checkRisks(const QVector<KLineData> &d, IndicatorResult &r)
{
    if (r.deathCross)
        r.warnings.append(QStringLiteral("MACD死叉，短期动能减弱"));
    if (r.kdjOverbought)
        r.warnings.append(QStringLiteral("KDJ超买(K>80)，注意回调风险"));
    if (r.kdjOversold)
        r.warnings.append(QStringLiteral("KDJ超卖(K<20)，存在超跌反弹机会"));
    if (r.rsiOverbought)
        r.warnings.append(QStringLiteral("RSI进入超买区域(>70)"));
    if (r.rsiOversold)
        r.warnings.append(QStringLiteral("RSI进入超卖区域(<30)"));
    if (r.maArrangement == QStringLiteral("空头排列"))
        r.warnings.append(QStringLiteral("均线空头排列，中期趋势偏弱"));
    if (r.volRatio < 0.6)
        r.warnings.append(QStringLiteral("成交量极度萎缩，关注变盘信号"));
    if (r.volRatio > 1.5 && qAbs(d.last().chgPct) < 1)
        r.warnings.append(QStringLiteral("放量滞涨/滞跌，多空分歧较大"));

    // MACD 顶背离检测（最近20日：价格新高，DIF新低）
    int n = d.size();
    if (n >= 25 && r.dif > 0) {
        double high20 = 0;
        int highIdx = 0;
        for (int i = n - 20; i < n; ++i) {
            if (d[i].high > high20) { high20 = d[i].high; highIdx = i; }
        }
        if (highIdx > n - 5 && d.last().close < high20 * 0.99) {
            // 价格接近前高但未突破，检查DIF走弱
            bool diverged = true;
            // 简化: 如果DIF最近5日趋势下行，且价格横盘/小幅上行
            // ... 实际实现较复杂，此处给出框架
            Q_UNUSED(diverged);
        }
    }

    // 连续缩量
    if (n >= 5) {
        bool consecutive = true;
        for (int i = n - 4; i < n; ++i)
            if (d[i].volume > d[i - 1].volume) consecutive = false;
        if (consecutive)
            r.warnings.append(QStringLiteral("连续缩量，市场参与度下降"));
    }
}

// ========== 一句话总结 ==========

void IndicatorEngine::buildSummary(const QVector<KLineData> &d, IndicatorResult &r)
{
    QStringList parts;
    const auto &last = d.last();

    // 涨跌
    if (last.chgPct >= 0)
        parts.append(QStringLiteral("收涨%1%").arg(last.chgPct, 0, 'f', 2));
    else
        parts.append(QStringLiteral("收跌%1%").arg(qAbs(last.chgPct), 0, 'f', 2));

    // MACD信号（优先级最高）
    if (r.deathCross)
        parts.append(QStringLiteral("MACD死叉"));
    else if (r.goldenCross)
        parts.append(QStringLiteral("MACD金叉"));
    else if (r.dif > r.dea)
        parts.append(QStringLiteral("MACD多头运行"));
    else
        parts.append(QStringLiteral("MACD空头运行"));

    // 均线
    parts.append(QStringLiteral("均线") + r.maArrangement);

    // 量价
    parts.append(r.volumeDesc);

    // 风险摘要
    if (!r.warnings.isEmpty())
        parts.append(QStringLiteral("注意: ") + r.warnings.first());
    else
        parts.append(QStringLiteral("暂无显著风险信号"));

    r.summary = parts.join(QStringLiteral("，")) + QStringLiteral("。");
}

// ========== 全序列 BOLL (20,2) ==========

void IndicatorEngine::bollSeries(const QVector<KLineData> &d,
                                 QVector<double> &up, QVector<double> &mid, QVector<double> &down)
{
    int n = d.size();
    up.resize(n); mid.resize(n); down.resize(n);
    for (int i = 0; i < n; ++i) {
        if (i < 19) { up[i] = mid[i] = down[i] = 0; continue; }
        double sum = 0;
        for (int j = i - 19; j <= i; ++j) sum += d[j].close;
        double m = sum / 20;
        double ss = 0;
        for (int j = i - 19; j <= i; ++j) {
            double diff = d[j].close - m;
            ss += diff * diff;
        }
        double sd = qSqrt(ss / 19);
        mid[i] = m;
        up[i]  = m + 2.0 * sd;
        down[i] = m - 2.0 * sd;
    }
}

// ========== 全序列 MACD (12,26,9) ==========

void IndicatorEngine::macdSeries(const QVector<KLineData> &d,
                                 QVector<double> &dif, QVector<double> &dea, QVector<double> &bar)
{
    int n = d.size();
    dif.resize(n); dea.resize(n); bar.resize(n);
    if (n < 26) return;

    // 初始 EMA12 / EMA26 用 SMA
    double init12 = 0, init26 = 0;
    for (int i = 0; i < 12; ++i) init12 += d[i].close;
    init12 /= 12;
    for (int i = 0; i < 26; ++i) init26 += d[i].close;
    init26 /= 26;

    double e12 = init12, e26 = init26;
    for (int i = 0; i < n; ++i) {
        e12 = ema(e12, d[i].close, 12);
        e26 = ema(e26, d[i].close, 26);
        dif[i] = e12 - e26;
    }

    // DEA = EMA9 of DIF
    double e9 = dif[0];
    dea[0] = e9;
    for (int i = 1; i < n; ++i) {
        e9 = ema(e9, dif[i], 9);
        dea[i] = e9;
        bar[i] = 2.0 * (dif[i] - dea[i]);
    }
}

// ========== 全序列 KDJ (9,3,3) ==========

void IndicatorEngine::kdjSeries(const QVector<KLineData> &d,
                                QVector<double> &k, QVector<double> &dv, QVector<double> &j)
{
    int n = d.size();
    k.resize(n); dv.resize(n); j.resize(n);
    double kv = 50, dv_v = 50;
    for (int i = 0; i < n; ++i) {
        if (i < 8) { k[i] = dv[i] = j[i] = 0; continue; }
        double lo = d[i].low, hi = d[i].high;
        for (int t = i - 8; t <= i; ++t) {
            if (d[t].low  < lo) lo = d[t].low;
            if (d[t].high > hi) hi = d[t].high;
        }
        double range = hi - lo;
        double rsv = (range < 1e-9) ? 50 : 100 * (d[i].close - lo) / range;
        kv   = 2.0/3.0 * kv   + 1.0/3.0 * rsv;
        dv_v = 2.0/3.0 * dv_v + 1.0/3.0 * kv;
        k[i]  = kv;
        dv[i] = dv_v;
        j[i]  = 3.0 * kv - 2.0 * dv_v;
    }
}
