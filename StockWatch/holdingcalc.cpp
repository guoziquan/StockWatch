#include "holdingcalc.h"

#include <algorithm>
#include <cmath>

namespace {

// 金额四舍五入到分,避免浮点累积误差
inline double round2(double v)
{
    return std::floor(v * 100.0 + 0.5) / 100.0;
}

// 稳定的合并排序:按日期升序;同日买入排在卖出之前
QVector<TradeRecord> sortRecords(const QVector<TradeRecord> &records)
{
    QVector<TradeRecord> out = records;
    std::stable_sort(out.begin(), out.end(), [](const TradeRecord &a, const TradeRecord &b) {
        if (a.date != b.date) return a.date < b.date;
        if (a.isBuy != b.isBuy) return a.isBuy;  // 买入在前
        return false;
    });
    return out;
}

} // namespace

// ==================== HoldingSummary ====================

double HoldingSummary::totalPnlPct() const
{
    // 分母用"净投入成本": 有持仓时用剩余成本基础,清仓时用总买入成本
    double base = (netShares > 0 && costBasis > 0) ? costBasis : totalBuyCost;
    if (base <= 0) return 0.0;
    return totalPnl / base * 100.0;
}

double HoldingSummary::unrealizedPnlPct() const
{
    if (costBasis <= 0.0) return 0.0;
    return unrealizedPnl / costBasis * 100.0;
}

// ==================== HoldingCalc ====================

TradeFee HoldingCalc::calcFee(bool isBuy, int shares, double price, const FeeConfig &cfg)
{
    TradeFee f;
    if (shares <= 0 || price <= 0.0) return f;

    const double amount = shares * price;

    // 佣金: 万2.5,最低 5 元
    f.commission = std::max(amount * cfg.commissionRate, cfg.minCommission);

    // 印花税: 仅卖出,0.05%(2023-08-28 起单边)
    if (!isBuy && cfg.stampTaxEnabled)
        f.stampTax = amount * cfg.stampTaxRate;

    // 过户费: 买卖双向,0.001%
    f.transfer = amount * cfg.transferRate;

    f.commission = round2(f.commission);
    f.stampTax   = round2(f.stampTax);
    f.transfer   = round2(f.transfer);
    return f;
}

QVector<TradeRecord> HoldingCalc::normalized(const QVector<TradeRecord> &records)
{
    QVector<TradeRecord> valid;
    valid.reserve(records.size());
    for (const auto &r : records) {
        if (r.shares > 0 && r.price > 0.0)
            valid.append(r);
    }
    return sortRecords(valid);
}

HoldingSummary HoldingCalc::compute(const QString &code,
                                    const QVector<TradeRecord> &records,
                                    double currentPrice,
                                    const FeeConfig &cfg)
{
    HoldingSummary s;
    s.code = code;

    const QVector<TradeRecord> recs = normalized(records);
    if (recs.isEmpty()) return s;

    s.valid = true;

    int    net        = 0;
    double costBasis  = 0.0;   // 剩余持仓成本基础(含买入费用)
    double realized   = 0.0;   // 已实现盈亏
    double sumBuyAmt  = 0.0;   // Σ 买入金额
    double sumSellAmt = 0.0;   // Σ 卖出金额
    int    totalBuyShares = 0;

    for (const auto &r : recs) {
        const TradeFee fee = calcFee(r.isBuy, r.shares, r.price, cfg);
        s.totalFee += fee.total();

        const double amount = r.shares * r.price;

        if (r.isBuy) {
            // 买入: 成本基础累加(含费用),成本价随之变化
            costBasis += amount + fee.total();
            net       += r.shares;
            sumBuyAmt += amount;
            totalBuyShares += r.shares;
            ++s.buyCount;
        } else {
            // 卖出: 卖出股数不能超过当前净持仓(超卖部分忽略,防止账本被污染)
            int sellShares = std::min(r.shares, net);
            if (sellShares <= 0) {
                sumSellAmt += amount;
                ++s.sellCount;
                continue;
            }

            // 按卖出前的单位成本价结转成本
            const double unitCost = (net > 0) ? costBasis / net : 0.0;
            const double costPart = unitCost * sellShares;

            // 卖出费用按实际卖出股数比例折算(超卖时)
            TradeFee realFee = fee;
            if (sellShares != r.shares) {
                const double ratio = double(sellShares) / double(r.shares);
                realFee.commission = round2(realFee.commission * ratio);
                realFee.stampTax   = round2(realFee.stampTax   * ratio);
                realFee.transfer   = round2(realFee.transfer   * ratio);
            }

            const double income = sellShares * r.price;

            realized  += income - costPart - realFee.total();
            costBasis -= costPart;          // 成本基础扣减,单位成本不变
            net       -= sellShares;
            sumSellAmt += amount;
            ++s.sellCount;

            // 清仓后成本基础归零(消除浮点残值)
            if (net == 0) costBasis = 0.0;
        }

        if (net < 0) net = 0;  // 理论上不会发生,兜底
    }

    s.netShares       = net;
    s.costBasis       = round2(costBasis);
    s.totalBuyCost    = round2(sumBuyAmt);
    s.totalSellIncome = round2(sumSellAmt);
    s.totalFee        = round2(s.totalFee);
    s.realizedPnl     = round2(realized);
    s.avgBuyPrice     = totalBuyShares > 0 ? sumBuyAmt / totalBuyShares : 0.0;

    if (net > 0) {
        s.costPrice     = costBasis / net;
        if (currentPrice > 0.0) {
            s.marketValue   = round2(net * currentPrice);
            s.unrealizedPnl = round2((currentPrice - s.costPrice) * net);
        }
    } else {
        s.costPrice = 0.0;
    }

    s.totalPnl = round2(s.realizedPnl + s.unrealizedPnl);
    return s;
}
