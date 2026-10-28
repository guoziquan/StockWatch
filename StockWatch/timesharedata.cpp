#include "timesharedata.h"

#include <QTime>

// ============================================================================
// 横轴槽位映射
//
// A 股交易时段: 09:30-11:30 (120 分钟) + 13:00-15:00 (120 分钟) = 240 格
// 约定：09:30 这一分钟落在 slot 0，11:30 落在 slot 119，
//       13:00 落在 slot 120，15:00 落在 slot 239。
// 这样午间休市不会在图中间留下空隙（横轴是"交易分钟"而非"自然分钟"）。
// ============================================================================

int TimeShareData::slotOf(const QString &time)
{
    // 取 "HH:mm" 部分（可能带日期前缀）
    QString hm = time;
    const int sp = hm.indexOf(QLatin1Char(' '));
    if (sp >= 0) hm = hm.mid(sp + 1);

    const QStringList parts = hm.split(QLatin1Char(':'));
    if (parts.size() < 2) return -1;
    const int h = parts[0].toInt();
    const int m = parts[1].toInt();
    const int minutes = h * 60 + m;

    const int AM_START = 9 * 60 + 30;    // 570
    const int AM_END   = 11 * 60 + 30;   // 690
    const int PM_START = 13 * 60;        // 780
    const int PM_END   = 15 * 60;        // 900

    if (minutes >= AM_START && minutes <= AM_END)
        return minutes - AM_START;                 // 0..120
    if (minutes >= PM_START && minutes <= PM_END)
        return 120 + (minutes - PM_START);         // 120..240

    return -1;   // 午休 / 盘前 / 盘后
}

QString TimeShareData::slotLabel(int slot)
{
    if (slot < 0 || slot > TOTAL_SLOTS) return QString();
    int minutes;
    if (slot <= 120)
        minutes = 9 * 60 + 30 + slot;
    else
        minutes = 13 * 60 + (slot - 120);

    const int h = minutes / 60;
    const int m = minutes % 60;
    return QStringLiteral("%1:%2").arg(h, 2, 10, QLatin1Char('0'))
                                  .arg(m, 2, 10, QLatin1Char('0'));
}

const MinutePoint &TimeShareData::last() const
{
    static MinutePoint empty;
    if (points.isEmpty()) return empty;
    return points.last();
}

// ============================================================================
// 解析
// ============================================================================
TimeShareData TimeShare::parse(const QString &code, const QString &name,
                               double prevClose, const QVector<QString> &lines)
{
    TimeShareData d;
    d.code = code;
    d.name = name;
    d.prevClose = prevClose;

    double runHigh = 0, runLow = 0;

    for (const QString &line : lines) {
        const QStringList f = line.split(QLatin1Char(','));
        if (f.size() < 7) continue;

        MinutePoint p;
        p.time = f[0].trimmed();          // "2026-10-28 09:30"
        // 只保留时间部分
        const int sp = p.time.indexOf(QLatin1Char(' '));
        if (sp >= 0) {
            if (d.date.isEmpty()) d.date = p.time.left(sp);
            p.time = p.time.mid(sp + 1);
        }

        // 字段: 时间,开,收,高,低,量,额,均价
        p.price  = f[2].toDouble();
        p.high   = f[3].toDouble();
        p.low    = f[4].toDouble();
        p.volume = f[5].toDouble();
        p.amount = f[6].toDouble();
        if (f.size() >= 8) p.avgPrice = f[7].toDouble();

        if (!p.valid()) continue;

        // 累计最高/最低（接口给的是分钟内的，这里按当日累计维护）
        if (runHigh <= 0 || p.high > runHigh) runHigh = p.high;
        if (runLow  <= 0 || p.low  < runLow)  runLow  = p.low;
        p.high = runHigh;
        p.low  = runLow;

        // 均价缺失时用 累计成交额 / 累计成交量 推算
        if (p.avgPrice <= 0) {
            double sumAmt = 0, sumVol = 0;
            for (const auto &q : d.points) { sumAmt += q.amount; sumVol += q.volume; }
            sumAmt += p.amount;
            sumVol += p.volume;
            if (sumVol > 0) p.avgPrice = sumAmt / sumVol;
        }

        d.points.append(p);
    }

    d.valid = !d.points.isEmpty() && prevClose > 0;
    return d;
}

double TimeShare::chgPct(double price, double prevClose)
{
    if (prevClose <= 0) return 0.0;
    return (price - prevClose) / prevClose * 100.0;
}

// ============================================================================
// 腾讯分时解析
//
// 源格式: "0930 16.11 1742 2806362.00"
//   时间(HHmm) 价格 累计成交量(手) 累计成交额(元)
//
// 关键转换：
//   * 每分钟成交量 = 当前累计量 - 上次累计量
//   * 每分钟成交额 = 当前累计额 - 上次累计额
//   * 均价 = 累计额 / (累计量 * 100)   ← 腾讯的"量"单位是手(100股)，
//                                        而"额"是元，需乘 100 换算成股
// ============================================================================
TimeShareData TimeShare::parseTencent(const QString &code, const QString &name,
                                      double prevClose, const QString &date,
                                      const QVector<QString> &lines)
{
    TimeShareData d;
    d.code = code;
    d.name = name;
    d.prevClose = prevClose;
    d.date = date;

    double prevCumVol = 0, prevCumAmt = 0;
    double runHigh = 0, runLow = 0;

    for (const QString &line : lines) {
        const QStringList f = line.split(QLatin1Char(' '), QString::SkipEmptyParts);
        if (f.size() < 2) continue;

        const QString hhmm = f[0].trimmed();
        if (hhmm.size() != 4) continue;

        MinutePoint p;
        p.time = QStringLiteral("%1:%2").arg(hhmm.left(2), hhmm.mid(2, 2));
        p.price = f[1].toDouble();
        if (!p.valid()) continue;

        const double cumVol = (f.size() >= 3) ? f[2].toDouble() : 0.0;
        const double cumAmt = (f.size() >= 4) ? f[3].toDouble() : 0.0;

        // 每分钟增量
        p.volume = qMax(0.0, cumVol - prevCumVol);
        p.amount = qMax(0.0, cumAmt - prevCumAmt);
        prevCumVol = cumVol;
        prevCumAmt = cumAmt;

        // 均价：累计额 / 累计股数（量单位为手，×100）
        if (cumVol > 0)
            p.avgPrice = cumAmt / (cumVol * 100.0);

        // 累计高低
        if (runHigh <= 0 || p.price > runHigh) runHigh = p.price;
        if (runLow  <= 0 || p.price < runLow)  runLow  = p.price;
        p.high = runHigh;
        p.low  = runLow;

        d.points.append(p);
    }

    d.valid = !d.points.isEmpty() && prevClose > 0;
    return d;
}
