#ifndef PRICELIMIT_H
#define PRICELIMIT_H

#include <QString>

// ============================================================================
// A 股涨跌停规则引擎
//
// 背景：A 股不同板块/不同状态下的涨跌幅限制差异很大，看盘时若不知道
// 涨停价/跌停价，就无法判断"现在是几板"、"距离涨停还有多少空间"。
//
// 规则汇总（截至 2026 年）：
//
// | 板块            | 代码特征        | 正常 | ST/*ST | 新股上市首日        |
// |-----------------|-----------------|------|--------|---------------------|
// | 主板(上证)      | 600/601/603/605 | ±10% | ±5%    | 首日±44%/-36% 次日±10% |
// | 主板(深证)      | 000/001         | ±10% | ±5%    | 首日±44%/-36% 次日±10% |
// | 创业板          | 300/301         | ±20% | ±20%   | 首日无限制 次日起±20% |
// | 科创板          | 688/689         | ±20% | ±20%   | 首日无限制 次日起±20% |
// | 北交所          | 43/83/87/920    | ±30% | ±30%   | 首日无限制 次日起±30% |
// | 科创板ST/风险警示| 688 且有ST      | ±20% |        |                     |
//
// 关键点（容易搞错）：
//   * 创业板 300/301 的 ST 股仍是 ±20%，不是 ±5%！
//     例如 ST佳缘(301117)：昨收 12.00 → 涨停 14.40 / 跌停 9.60
//   * 科创板 688 的 ST 股同样是 ±20%
//   * 只有主板（600/601/603/605/000/001）的 ST 股才是 ±5%
//
// 价格计算规则（交易所规定）：四舍五入到 0.01 元
// ============================================================================

// ---------- 交易板块 ----------
enum class BoardType {
    Unknown,
    MainSh,      // 沪市主板 600/601/603/605
    MainSz,      // 深市主板 000/001
    ChiNext,     // 创业板 300/301
    StarMarket,  // 科创板 688/689
    BSE          // 北交所 43/83/87/920
};

// ---------- 风险警示状态 ----------
enum class RiskStatus {
    Normal,        // 正常
    ST,            // 其他风险警示 (ST)
    StarST         // 退市风险警示 (*ST)
};

// ---------- 涨跌停计算结果 ----------
struct PriceLimit {
    BoardType   board        = BoardType::Unknown;
    RiskStatus  risk         = RiskStatus::Normal;
    double      limitPct     = 10.0;   // 涨跌幅限制(%)
    double      upPrice      = 0.0;    // 涨停价
    double      downPrice    = 0.0;    // 跌停价
    bool        valid        = false;  // 计算是否有效
    bool        noLimit      = false;  // 是否无涨跌幅限制(创业板/科创板新股首日)
    QString     explanation;           // 规则说明，供 UI 展示
};

namespace PriceLimitCalc {

// 根据代码判断板块
BoardType boardOf(const QString &code);

// 根据名称判断风险警示状态（"ST"/"*ST" 前缀，或名称含 ST）
RiskStatus riskStatusOf(const QString &name);

// 计算涨跌幅限制百分比
double limitPctOf(BoardType board, RiskStatus risk);

// 核心：按昨收价计算涨跌停价
//   prevClose <= 0 时返回无效结果
PriceLimit compute(const QString &code, const QString &name, double prevClose);

// 判断当前价是否已涨停/跌停（考虑 0.01 元容差）
bool isLimitUp(double price, const PriceLimit &pl);
bool isLimitDown(double price, const PriceLimit &pl);

// 距离涨停/跌停的剩余百分比（正数表示还有空间）
double distToUpPct(double price, const PriceLimit &pl);
double distToDownPct(double price, const PriceLimit &pl);

// 板块中文名
QString boardName(BoardType b);
// 风险状态中文名
QString riskName(RiskStatus r);

} // namespace PriceLimitCalc

#endif // PRICELIMIT_H
