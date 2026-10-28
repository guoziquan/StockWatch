#ifndef TRADINGCALENDAR_H
#define TRADINGCALENDAR_H

#include <QDate>
#include <QString>
#include <QVector>

// ============================================================================
// A 股交易日历
//
// 解决的问题:
//   原实现仅用 dayOfWeek()>=6 判断休市,完全忽略了法定节假日。
//   后果: 春节/国庆等长假期间,程序仍显示"🟢 交易中"并持续轮询行情,
//         用户看到"更新于 HH:MM:SS"正常跳动,会误以为拿到的是实时数据,
//         实际是上一交易日收盘价 —— 这是误导性最强的缺陷。
//
// 本模块提供:
//   * 2025~2027 年沪深交易所休市日(法定节假日 + 调休)
//   * isTradingDay / isHoliday 判断
//   * nextTradingDay / prevTradingDay 推算
//   * 会话状态(集合竞价/连续竞价/午间休市/已收盘)
// ============================================================================

// ---------- 盘中阶段 ----------
enum class SessionPhase {
    Closed,          // 已收盘 / 非交易日
    PreOpen,         // 早于 9:15
    CallAuction,     // 集合竞价 9:15-9:25
    CallAuctionEnd,  // 9:25-9:30 撮合结束等待开盘
    MorningTrading,  // 9:30-11:30
    LunchBreak,      // 11:30-13:00
    AfternoonTrading,// 13:00-15:00
    AfterClose       // 15:00 之后
};

namespace TradingCalendar {

// 是否交易日(排除周末与法定休市日)
bool isTradingDay(const QDate &d);

// 是否处于休市日(周末或法定假日);节假日返回 true,普通周末也返回 true
bool isHoliday(const QDate &d);

// 若为法定节假日,返回节日名称(如"春节");否则返回空字符串
QString holidayName(const QDate &d);

// 下一个交易日(严格大于 d)
QDate nextTradingDay(const QDate &d);
// 上一个交易日(严格小于 d)
QDate prevTradingDay(const QDate &d);

// 给定日期时间,返回盘中阶段
SessionPhase phaseAt(const QDateTime &t);

// 当前阶段的中文描述;若为休市,说明下一个交易日
QString describe(const QDateTime &t);

// 该年份的休市日表(调试/展示用)
QVector<QDate> holidaysOf(int year);

// 日历是否覆盖指定年份(未覆盖时 isTradingDay 会退化为"仅排除周末",应提示用户更新)
bool coversYear(int year);

} // namespace TradingCalendar

#endif // TRADINGCALENDAR_H
