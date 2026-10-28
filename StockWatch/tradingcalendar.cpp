#include "tradingcalendar.h"

#include <QDateTime>
#include <QMap>

namespace {

// ============================================================================
// 休市日数据
//
// 数据来源: 上海/深圳证券交易所每年发布的《关于XXXX年节假日休市安排的通知》,
// 按"全体公民放假的节日"执行。表中为实际休市日(不含正常周末,
// 但含因调休而额外休市的周六/周日及调休上班日不做特殊标记——A股从不调休上班)。
//
// 说明: A 股不实行周末调休上班,所以"调休上班的周末"在 A 股仍然是休市。
//       因此本表只需记录"非法定周末的额外休市日",周末由 isWeekend 统一处理。
//
// 维护方式: 每年 12 月中下旬交易所发布次年安排后,在下方追加即可。
//           未覆盖年份会退化为"仅排除周末",并在 UI 上给出更新提示。
// ============================================================================

struct HolidayRange {
    QDate   from;
    QDate   to;
    QString name;
};

const HolidayRange g_ranges[] = {
    // ---------------- 2025 ----------------
    { QDate(2025, 1,  1), QDate(2025, 1,  1), QStringLiteral("元旦") },
    { QDate(2025, 1, 28), QDate(2025, 2,  4), QStringLiteral("春节") },
    { QDate(2025, 4,  4), QDate(2025, 4,  6), QStringLiteral("清明") },
    { QDate(2025, 5,  1), QDate(2025, 5,  5), QStringLiteral("劳动节") },
    { QDate(2025, 5, 31), QDate(2025, 6,  2), QStringLiteral("端午") },
    { QDate(2025, 10, 1), QDate(2025, 10, 8), QStringLiteral("国庆·中秋") },

    // ---------------- 2026 ----------------
    { QDate(2026, 1,  1), QDate(2026, 1,  2), QStringLiteral("元旦") },
    { QDate(2026, 2, 16), QDate(2026, 2, 22), QStringLiteral("春节") },
    { QDate(2026, 4,  6), QDate(2026, 4,  6), QStringLiteral("清明") },
    { QDate(2026, 5,  1), QDate(2026, 5,  5), QStringLiteral("劳动节") },
    { QDate(2026, 6, 19), QDate(2026, 6, 19), QStringLiteral("端午") },
    { QDate(2026, 9, 25), QDate(2026, 9, 25), QStringLiteral("中秋") },
    { QDate(2026, 10, 1), QDate(2026, 10, 7), QStringLiteral("国庆") },

    // ---------------- 2027 ----------------
    // 预排(按农历推算 + 近年规律),交易所正式公告后需复核
    { QDate(2027, 1,  1), QDate(2027, 1,  3), QStringLiteral("元旦") },
    { QDate(2027, 2,  6), QDate(2027, 2, 12), QStringLiteral("春节") },
    { QDate(2027, 4,  5), QDate(2027, 4,  5), QStringLiteral("清明") },
    { QDate(2027, 5,  1), QDate(2027, 5,  5), QStringLiteral("劳动节") },
    { QDate(2027, 6,  9), QDate(2027, 6,  9), QStringLiteral("端午") },
    { QDate(2027, 9, 15), QDate(2027, 9, 15), QStringLiteral("中秋") },
    { QDate(2027, 10, 1), QDate(2027, 10, 7), QStringLiteral("国庆") },
};

const int g_rangeCount = int(sizeof(g_ranges) / sizeof(g_ranges[0]));
const int g_firstYear = 2025;
const int g_lastYear  = 2027;

const HolidayRange *findRange(const QDate &d)
{
    for (int i = 0; i < g_rangeCount; ++i) {
        if (d >= g_ranges[i].from && d <= g_ranges[i].to)
            return &g_ranges[i];
    }
    return nullptr;
}

inline bool isWeekend(const QDate &d)
{
    return d.dayOfWeek() >= 6;   // 6=周六, 7=周日
}

} // namespace

namespace TradingCalendar {

bool coversYear(int year)
{
    return year >= g_firstYear && year <= g_lastYear;
}

bool isHoliday(const QDate &d)
{
    if (!d.isValid()) return true;
    if (isWeekend(d)) return true;
    return findRange(d) != nullptr;
}

bool isTradingDay(const QDate &d)
{
    if (!d.isValid()) return false;
    if (isWeekend(d)) return false;
    if (!coversYear(d.year())) {
        // 数据未覆盖: 退化处理,仅排除周末。宁可少报节假日,
        // 也不要把真实交易日误判成休市(后者会阻止正常刷新)。
        return true;
    }
    return findRange(d) == nullptr;
}

QString holidayName(const QDate &d)
{
    if (!d.isValid()) return QString();
    const HolidayRange *r = findRange(d);
    return r ? r->name : QString();
}

QDate nextTradingDay(const QDate &d)
{
    if (!d.isValid()) return QDate();
    QDate cur = d.addDays(1);
    // 安全上限: 一年足够覆盖最长春节休市
    for (int i = 0; i < 400; ++i) {
        if (isTradingDay(cur)) return cur;
        cur = cur.addDays(1);
    }
    return QDate();
}

QDate prevTradingDay(const QDate &d)
{
    if (!d.isValid()) return QDate();
    QDate cur = d.addDays(-1);
    for (int i = 0; i < 400; ++i) {
        if (isTradingDay(cur)) return cur;
        cur = cur.addDays(-1);
    }
    return QDate();
}

SessionPhase phaseAt(const QDateTime &t)
{
    if (!t.isValid()) return SessionPhase::Closed;
    const QDate d = t.date();
    if (!isTradingDay(d)) return SessionPhase::Closed;

    const QTime tm = t.time();
    const int hm = tm.hour() * 60 + tm.minute();

    if (hm < 9 * 60 + 15)  return SessionPhase::PreOpen;
    if (hm < 9 * 60 + 25)  return SessionPhase::CallAuction;
    if (hm < 9 * 60 + 30)  return SessionPhase::CallAuctionEnd;
    if (hm < 11 * 60 + 30) return SessionPhase::MorningTrading;
    if (hm < 13 * 60)      return SessionPhase::LunchBreak;
    if (hm < 15 * 60)      return SessionPhase::AfternoonTrading;
    return SessionPhase::AfterClose;
}

QString describe(const QDateTime &t)
{
    if (!t.isValid()) return QStringLiteral("--");
    const QDate d = t.date();
    const SessionPhase ph = phaseAt(t);
    const QTime tm = t.time();
    const int hm = tm.hour() * 60 + tm.minute();

    auto fmtHM = [](int minutes) {
        return QStringLiteral("%1:%2").arg(minutes / 60)
                                      .arg(minutes % 60, 2, 10, QChar('0'));
    };

    // ---------- 非交易日 ----------
    if (!isTradingDay(d)) {
        const QDate nd = nextTradingDay(d);
        const QString ndStr = nd.isValid()
            ? nd.toString(QStringLiteral("M月d日")) : QStringLiteral("--");
        const int days = d.daysTo(nd);

        if (isWeekend(d) && holidayName(d).isEmpty()) {
            return QStringLiteral("休市(周末) · %1 天后 %2 9:30 开盘")
                .arg(days).arg(ndStr);
        }
        const QString name = holidayName(d);
        const QString label = name.isEmpty() ? QStringLiteral("节假日") : name;
        return QStringLiteral("休市(%1) · %2 天后 %3 9:30 开盘")
            .arg(label).arg(days).arg(ndStr);
    }

    // ---------- 交易日 ----------
    switch (ph) {
    case SessionPhase::PreOpen:
        return QStringLiteral("未开盘 · 今日 9:15 集合竞价");
    case SessionPhase::CallAuction:
        return QStringLiteral("集合竞价中 · 9:25 结束");
    case SessionPhase::CallAuctionEnd:
        return QStringLiteral("集合竞价结束 · %1 后开盘").arg(fmtHM(9 * 60 + 30 - hm));
    case SessionPhase::MorningTrading:
        return QStringLiteral("🟢 交易中 · %1 后午休").arg(fmtHM(11 * 60 + 30 - hm));
    case SessionPhase::LunchBreak:
        return QStringLiteral("☕ 午间休市 · %1 后开盘").arg(fmtHM(13 * 60 - hm));
    case SessionPhase::AfternoonTrading:
        return QStringLiteral("🟢 交易中 · %1 后收盘").arg(fmtHM(15 * 60 - hm));
    case SessionPhase::AfterClose: {
        const QDate nd = nextTradingDay(d);
        // 若明天就是交易日,直接说"明日";否则给出日期
        if (nd == d.addDays(1))
            return QStringLiteral("🔴 已收盘 · 明日 9:30 开盘");
        if (isWeekend(nd) == false && nd.isValid())
            return QStringLiteral("🔴 已收盘 · %1 9:30 开盘")
                .arg(nd.toString(QStringLiteral("M月d日")));
        return QStringLiteral("🔴 已收盘 · %1 9:30 开盘")
            .arg(nd.isValid() ? nd.toString(QStringLiteral("M月d日")) : QStringLiteral("--"));
    }
    case SessionPhase::Closed:
        break;
    }
    return QStringLiteral("已收盘");
}

QVector<QDate> holidaysOf(int year)
{
    QVector<QDate> out;
    for (int i = 0; i < g_rangeCount; ++i) {
        if (g_ranges[i].from.year() > year || g_ranges[i].to.year() < year) continue;
        QDate cur = g_ranges[i].from;
        while (cur <= g_ranges[i].to) {
            if (cur.year() == year && !isWeekend(cur))
                out.append(cur);
            cur = cur.addDays(1);
        }
    }
    return out;
}

} // namespace TradingCalendar
