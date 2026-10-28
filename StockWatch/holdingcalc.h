#ifndef HOLDINGCALC_H
#define HOLDINGCALC_H

#include <QString>
#include <QDate>
#include <QVector>

// ============================================================================
// 持仓账本计算引擎
//
// 采用 A 股券商标准的【移动加权平均法】(Moving Weighted Average):
//   - 买入: 成本基础 += 买入股数 × 买入价 + 买入费用
//          持仓成本价 = 成本基础 / 净持仓
//   - 卖出: 实现盈亏 += 卖出股数 × (卖出价 - 卖出前持仓成本价) - 卖出费用
//          成本基础 -= 卖出股数 × 卖出前持仓成本价   (成本价保持不变)
//          持仓成本价 = 成本基础 / 净持仓             (卖出后成本价恒定)
//
// 与旧实现的差异(旧实现 = 当前市值 - 总买入成本 + 总卖出收入):
//   旧公式把"卖出收入"整额计入盈亏,不做成本结转,导致:
//     * "均价"显示的是历次买入的加权平均,而非剩余持仓的真实成本
//     * 减仓/T 之后无法回答"我现在成本价多少"这个最关键的问题
//     * 部分卖出后,浮动盈亏与已实现盈亏无法分离
//
// 本引擎同时提供:
//   * 完整的 A 股交易费用模型(佣金/印花税/过户费)
//   * 已实现盈亏 / 浮动盈亏 / 累计盈亏 三者的正确分离
//   * 含费净盈亏(旧实现完全没有费用概念)
// ============================================================================

// ---------- 单笔交易记录 ----------
struct TradeRecord {
    QString code;
    bool    isBuy  = true;   // true=买入, false=卖出
    int     shares = 0;      // 股数(始终 >0)
    double  price  = 0.0;    // 成交价
    QDate   date;
};

// ---------- 费用模型 ----------
struct FeeConfig {
    double commissionRate = 0.00025;  // 佣金费率 万2.5
    double minCommission  = 5.0;      // 最低佣金 5 元
    double stampTaxRate   = 0.0005;   // 印花税 0.05%(仅卖出)
    double transferRate   = 0.00001;  // 过户费 0.001%(沪深两市均收)
    bool   stampTaxEnabled = true;    // 印花税开关(备用)
};

// ---------- 单笔交易费用明细 ----------
struct TradeFee {
    double commission = 0.0;  // 佣金(已应用最低值)
    double stampTax   = 0.0;  // 印花税(仅卖出)
    double transfer   = 0.0;  // 过户费
    double total() const { return commission + stampTax + transfer; }
};

// ---------- 持仓汇总结果 ----------
struct HoldingSummary {
    QString code;
    int     netShares      = 0;      // 净持仓股数
    double  costBasis      = 0.0;    // 剩余持仓成本基础(含买入费用)
    double  costPrice      = 0.0;    // 持仓成本价 = costBasis / netShares
    double  avgBuyPrice    = 0.0;    // 历次买入的算术加权均价(仅参考,非成本价)
    double  totalBuyCost   = 0.0;    // Σ 买入金额(不含费用)
    double  totalSellIncome= 0.0;    // Σ 卖出金额(不含费用)
    double  totalFee       = 0.0;    // Σ 全部交易费用
    double  realizedPnl    = 0.0;    // 已实现盈亏(已扣卖出费用与对应买入成本)
    double  unrealizedPnl  = 0.0;    // 浮动盈亏 = (现价-成本价)×净持仓
    double  marketValue    = 0.0;    // 当前市值 = 净持仓 × 现价
    double  totalPnl       = 0.0;    // 累计盈亏 = 已实现 + 浮动
    int     buyCount       = 0;
    int     sellCount      = 0;
    bool    valid          = false;  // 是否有有效记录

    // 累计盈亏率(相对总投入成本),无持仓或成本为0时返回0
    double totalPnlPct() const;
    // 浮动盈亏率(相对当前成本基础)
    double unrealizedPnlPct() const;
};

namespace HoldingCalc {

// 计算单笔交易费用
TradeFee calcFee(bool isBuy, int shares, double price, const FeeConfig &cfg = FeeConfig());

// 按时间顺序(同一日期内买入优先于卖出,保证先建仓后减仓)规整记录顺序
QVector<TradeRecord> normalized(const QVector<TradeRecord> &records);

// 核心: 按移动加权平均法汇总
// currentPrice <= 0 时浮动盈亏/市值为 0
HoldingSummary compute(const QString &code,
                       const QVector<TradeRecord> &records,
                       double currentPrice = 0.0,
                       const FeeConfig &cfg = FeeConfig());

} // namespace HoldingCalc

#endif // HOLDINGCALC_H
