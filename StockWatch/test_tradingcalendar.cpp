// 交易日历验证程序（离线）
#include "tradingcalendar.h"
#include <QCoreApplication>
#include <QDateTime>
#include <cstdio>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, bool ok)
{
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", name);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    printf("=== 交易日 / 休市日判断 ===\n");

    // ---- 2026 年关键日期 ----
    // 2026-10-26 是周一，普通交易日
    check("2026-10-26 周一 是交易日", TradingCalendar::isTradingDay(QDate(2026, 10, 26)));
    check("2026-10-24 周六 非交易日", !TradingCalendar::isTradingDay(QDate(2026, 10, 24)));
    check("2026-10-25 周日 非交易日", !TradingCalendar::isTradingDay(QDate(2026, 10, 25)));

    // 国庆 2026-10-01 ~ 10-07（周四到周三）全部休市
    bool allClosed = true;
    for (int d = 1; d <= 7; ++d)
        if (TradingCalendar::isTradingDay(QDate(2026, 10, d))) allClosed = false;
    check("2026 国庆 10/01-10/07 全部休市", allClosed);
    check("2026-10-01 识别为国庆", TradingCalendar::holidayName(QDate(2026, 10, 1)) == QStringLiteral("国庆"));
    check("2026-10-08 国庆后首日开市", TradingCalendar::isTradingDay(QDate(2026, 10, 8)));

    // 春节 2026-02-16 ~ 02-22
    check("2026-02-16 春节休市", !TradingCalendar::isTradingDay(QDate(2026, 2, 16)));
    check("2026-02-16 识别为春节", TradingCalendar::holidayName(QDate(2026, 2, 16)) == QStringLiteral("春节"));
    check("2026-02-23 春节后首日开市", TradingCalendar::isTradingDay(QDate(2026, 2, 23)));

    // 中秋 2026-09-25 周五
    check("2026-09-25 中秋休市", !TradingCalendar::isTradingDay(QDate(2026, 9, 25)));
    // 2026-09-24 周四应为交易日
    check("2026-09-24 周四 交易", TradingCalendar::isTradingDay(QDate(2026, 9, 24)));

    printf("\n=== 前后交易日推算 ===\n");
    check("国庆前最后交易日 = 2026-09-30",
          TradingCalendar::prevTradingDay(QDate(2026, 10, 1)) == QDate(2026, 9, 30));
    check("国庆后第一个交易日 = 2026-10-08",
          TradingCalendar::nextTradingDay(QDate(2026, 10, 7)) == QDate(2026, 10, 8));
    check("周五之后是周一 2026-10-30 -> 11-02",
          TradingCalendar::nextTradingDay(QDate(2026, 10, 30)) == QDate(2026, 11, 2));

    printf("\n=== 盘中阶段 ===\n");
    const QDate D(2026, 10, 26); // 周一，交易日
    auto at = [&](int h, int m) { return QDateTime(D, QTime(h, m)); };

    check("08:00 PreOpen",           TradingCalendar::phaseAt(at(8, 0))  == SessionPhase::PreOpen);
    check("09:10 PreOpen",           TradingCalendar::phaseAt(at(9, 10)) == SessionPhase::PreOpen);
    check("09:15 CallAuction",       TradingCalendar::phaseAt(at(9, 15)) == SessionPhase::CallAuction);
    check("09:20 CallAuction",       TradingCalendar::phaseAt(at(9, 20)) == SessionPhase::CallAuction);
    check("09:26 CallAuctionEnd",    TradingCalendar::phaseAt(at(9, 26)) == SessionPhase::CallAuctionEnd);
    check("09:30 MorningTrading",    TradingCalendar::phaseAt(at(9, 30)) == SessionPhase::MorningTrading);
    check("10:30 MorningTrading",    TradingCalendar::phaseAt(at(10, 30))== SessionPhase::MorningTrading);
    check("11:29 MorningTrading",    TradingCalendar::phaseAt(at(11, 29))== SessionPhase::MorningTrading);
    check("11:30 LunchBreak",        TradingCalendar::phaseAt(at(11, 30))== SessionPhase::LunchBreak);
    check("12:15 LunchBreak",        TradingCalendar::phaseAt(at(12, 15))== SessionPhase::LunchBreak);
    check("13:00 AfternoonTrading",  TradingCalendar::phaseAt(at(13, 0)) == SessionPhase::AfternoonTrading);
    check("14:59 AfternoonTrading",  TradingCalendar::phaseAt(at(14, 59))== SessionPhase::AfternoonTrading);
    check("15:00 AfterClose",        TradingCalendar::phaseAt(at(15, 0)) == SessionPhase::AfterClose);
    check("20:00 AfterClose",        TradingCalendar::phaseAt(at(20, 0)) == SessionPhase::AfterClose);

    // 休市日的任何时刻都是 Closed
    check("国庆期间 10:00 = Closed",
          TradingCalendar::phaseAt(QDateTime(QDate(2026, 10, 2), QTime(10, 0))) == SessionPhase::Closed);

    printf("\n=== 描述文字（人工核对可读性）===\n");
    printf("  2026-10-26 10:30  -> %s\n", qPrintable(TradingCalendar::describe(at(10, 30))));
    printf("  2026-10-26 12:15  -> %s\n", qPrintable(TradingCalendar::describe(at(12, 15))));
    printf("  2026-10-26 14:30  -> %s\n", qPrintable(TradingCalendar::describe(at(14, 30))));
    printf("  2026-10-26 20:00  -> %s\n", qPrintable(TradingCalendar::describe(at(20, 0))));
    printf("  2026-10-26 08:00  -> %s\n", qPrintable(TradingCalendar::describe(at(8, 0))));
    printf("  2026-10-25 10:00  -> %s\n", qPrintable(TradingCalendar::describe(QDateTime(QDate(2026,10,25), QTime(10,0)))));
    printf("  2026-10-02 10:00  -> %s\n", qPrintable(TradingCalendar::describe(QDateTime(QDate(2026,10,2), QTime(10,0)))));
    printf("  2026-09-30 16:00  -> %s\n", qPrintable(TradingCalendar::describe(QDateTime(QDate(2026,9,30), QTime(16,0)))));
    printf("  2026-02-13 16:00  -> %s\n", qPrintable(TradingCalendar::describe(QDateTime(QDate(2026,2,13), QTime(16,0)))));

    printf("\n=== 覆盖年份 ===\n");
    check("2025 已覆盖", TradingCalendar::coversYear(2025));
    check("2026 已覆盖", TradingCalendar::coversYear(2026));
    check("2027 已覆盖", TradingCalendar::coversYear(2027));
    check("2028 未覆盖", !TradingCalendar::coversYear(2028));
    // 未覆盖年份退化为"仅排除周末"，不能把工作日误判为休市
    check("2030-01-02(周三) 未覆盖年份仍视为交易日",
          TradingCalendar::isTradingDay(QDate(2030, 1, 2)));
    check("2030-01-05(周六) 仍为休市",
          !TradingCalendar::isTradingDay(QDate(2030, 1, 5)));

    printf("\n=== 2026 年休市统计 ===\n");
    QVector<QDate> h = TradingCalendar::holidaysOf(2026);
    printf("  2026 年非法定周末的休市日共 %d 天\n", h.size());
    check("2026 休市日数量在合理范围(15-25)", h.size() >= 15 && h.size() <= 25);

    printf("\n===== 结果: %d 通过, %d 失败 =====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
