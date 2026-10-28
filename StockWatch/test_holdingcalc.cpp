// 持仓账本算法验证程序（离线，不依赖 Qt GUI，不需要网络）
// 编译: g++ -std=gnu++11 test_holdingcalc.cpp holdingcalc.cpp -o test_holdingcalc.exe
//      需要 Qt5Core 的 include 与 lib（QString/QDate）
#include "holdingcalc.h"
#include <QCoreApplication>
#include <QDate>
#include <cstdio>
#include <cmath>

static int g_pass = 0, g_fail = 0;

static void near(const char *name, double got, double want, double eps = 0.011)
{
    const bool ok = std::fabs(got - want) <= eps;
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %-42s got=%14.4f  want=%14.4f\n",
           ok ? "[PASS]" : "[FAIL]", name, got, want);
}

static void eq(const char *name, int got, int want)
{
    const bool ok = (got == want);
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %-42s got=%14d  want=%14d\n",
           ok ? "[PASS]" : "[FAIL]", name, got, want);
}

// 关闭全部费用，用于验证纯股份/成本逻辑
static FeeConfig noFee()
{
    FeeConfig c;
    c.commissionRate = 0.0; c.minCommission = 0.0;
    c.stampTaxRate = 0.0;   c.transferRate = 0.0;
    return c;
}

static TradeRecord buy(int shares, double price, const QDate &d)
{
    TradeRecord r; r.code = "000000"; r.isBuy = true; r.shares = shares; r.price = price; r.date = d;
    return r;
}
static TradeRecord sell(int shares, double price, const QDate &d)
{
    TradeRecord r; r.code = "000000"; r.isBuy = false; r.shares = shares; r.price = price; r.date = d;
    return r;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QDate d0(2026, 1, 5), d1(2026, 1, 6), d2(2026, 1, 7), d3(2026, 1, 8);

    // ---- 用例 1: 纯买入，再按市价计算浮动盈亏 ----
    {
        QVector<TradeRecord> r{ buy(1000, 10.0, d0) };
        HoldingSummary s = HoldingCalc::compute("A", r, 12.0, noFee());
        eq  ("1 净持仓", s.netShares, 1000);
        near("1 成本价", s.costPrice, 10.0);
        near("1 浮动盈亏", s.unrealizedPnl, 2000.0);
        near("1 已实现盈亏", s.realizedPnl, 0.0);
        near("1 累计盈亏", s.totalPnl, 2000.0);
    }

    // ---- 用例 2: 部分卖出后成本价必须保持不变（移动加权核心）----
    // 买 1000@10，卖 900@12，剩 100 股，成本价仍应为 10
    {
        QVector<TradeRecord> r{ buy(1000, 10.0, d0), sell(900, 12.0, d1) };
        HoldingSummary s = HoldingCalc::compute("A", r, 8.0, noFee());
        eq  ("2 净持仓", s.netShares, 100);
        near("2 成本价(应保持10)", s.costPrice, 10.0);
        near("2 已实现盈亏", s.realizedPnl, 1800.0);
        near("2 浮动盈亏", s.unrealizedPnl, -200.0);
        near("2 累计盈亏", s.totalPnl, 1600.0);   // 旧公式在此也会得1600
    }

    // ---- 用例 3: 关键反例——现价下跌时旧公式给出错误答案 ----
    // 买 1000@10，卖 900@15（大赚），剩 100 股现价 5
    // 正确: 已实现 (15-10)*900=4500, 浮动 (5-10)*100=-500, 累计=4000
    // 旧公式: 100*5 - 10000 + 13500 = 4000  -> 巧合相同
    // 因此改用"多次买入"来构造真正差异：
    {
        // 买 1000@10，再买 1000@20（成本价变 15），卖 1000@25
        // 正确: 已实现 (25-15)*1000 = 10000；剩 1000 股成本价仍 15
        // 旧公式: 成本均价 = (1000*10+1000*20)/2000 = 15，
        //         盈亏 = 现价*1000 - 30000 + 25000
        // 现价取 10: 正确 = 10000 + (10-15)*1000 = 5000
        //             旧   = 10*1000 - 30000 + 25000 = 5000  -> 仍相同
        // 说明：只要"卖出价 == 卖出前成本价"，两者恒等。
        // 真正的差异出现在卖出价 != 卖出前成本价 且后续价格变动时，
        // 因为旧公式不做成本结转，剩余成本被"稀释"。
    }

    // ---- 用例 4: 真正区分新旧算法的场景 ----
    // 买 1000@10（成本价10），卖 500@10（平价卖出），再买 1000@30
    // 买入后成本价 = (500*10 + 1000*30)/1500 = 35000/1500 = 23.3333
    // 旧实现"均价" = 总买入成本/总买入股数 = (10000+30000)/2000 = 20
    //   -> 旧"均价"20 完全错误，应为 23.3333
    {
        QVector<TradeRecord> r{ buy(1000, 10.0, d0), sell(500, 10.0, d1), buy(1000, 30.0, d2) };
        HoldingSummary s = HoldingCalc::compute("A", r, 25.0, noFee());
        eq  ("4 净持仓", s.netShares, 1500);
        near("4 成本价(正确23.333)", s.costPrice, 35000.0 / 1500.0);
        near("4 均价(仅参考,应为20)", s.avgBuyPrice, 20.0);
        near("4 已实现盈亏", s.realizedPnl, 0.0);
        near("4 浮动盈亏", s.unrealizedPnl, 2500.0);   // (25-23.3333)*1500
        near("4 累计盈亏", s.totalPnl, 2500.0);
    }

    // ---- 用例 5: 清仓后累计盈亏 = 已实现盈亏，浮动归零 ----
    {
        QVector<TradeRecord> r{ buy(1000, 10.0, d0), sell(1000, 12.0, d1) };
        HoldingSummary s = HoldingCalc::compute("A", r, 99.0, noFee());
        eq  ("5 净持仓(清仓)", s.netShares, 0);
        near("5 成本价(清仓)", s.costPrice, 0.0);
        near("5 浮动盈亏(清仓)", s.unrealizedPnl, 0.0);
        near("5 已实现盈亏", s.realizedPnl, 2000.0);
        near("5 累计盈亏", s.totalPnl, 2000.0);
    }

    // ---- 用例 6: A 股默认费用模型 ----
    // 买 1000@10 = 10000 元：佣金 max(2.5,5)=5，过户 0.1 -> 5.1
    // 卖 1000@12 = 12000 元：佣金 max(3,5)=5，印花 6.0，过户 0.12 -> 11.12
    {
        TradeFee fb = HoldingCalc::calcFee(true, 1000, 10.0, FeeConfig());
        near("6 买入佣金(最低5元)", fb.commission, 5.0);
        near("6 买入印花税(免)", fb.stampTax, 0.0);
        near("6 买入过户费", fb.transfer, 0.10);
        near("6 买入费用合计", fb.total(), 5.10);

        TradeFee fs = HoldingCalc::calcFee(false, 1000, 12.0, FeeConfig());
        near("6 卖出佣金", fs.commission, 5.0);
        near("6 卖出印花税", fs.stampTax, 6.0);
        near("6 卖出过户费", fs.transfer, 0.12);
        near("6 卖出费用合计", fs.total(), 11.12);
    }

    // ---- 用例 7: 含费盈亏，应小于不含费盈亏 ----
    {
        QVector<TradeRecord> r{ buy(1000, 10.0, d0), sell(1000, 12.0, d1) };
        HoldingSummary free = HoldingCalc::compute("A", r, 12.0, noFee());
        HoldingSummary paid = HoldingCalc::compute("A", r, 12.0, FeeConfig());
        near("7 不含费已实现", free.realizedPnl, 2000.0);
        // 含费: 卖出收入 - (成本基础含买入费) - 卖出费
        //      = 12000 - (10000+5.10) - 11.12 = 1983.78
        near("7 含费已实现", paid.realizedPnl, 1983.78);
        near("7 累计费用", paid.totalFee, 5.10 + 11.12);
        eq  ("7 含费盈亏 < 不含费", paid.realizedPnl < free.realizedPnl ? 1 : 0, 1);
    }

    // ---- 用例 8: 无记录 / 非法记录 ----
    {
        HoldingSummary empty = HoldingCalc::compute("A", QVector<TradeRecord>(), 10.0, FeeConfig());
        eq  ("8 空记录 valid", empty.valid ? 1 : 0, 0);
        eq  ("8 空记录净持仓", empty.netShares, 0);

        QVector<TradeRecord> bad{ buy(0, 10.0, d0), buy(100, 0.0, d0) };
        HoldingSummary s = HoldingCalc::compute("A", bad, 10.0, FeeConfig());
        eq  ("8 非法记录被过滤", s.valid ? 1 : 0, 0);
    }

    // ---- 用例 9: 超卖保护（卖出多于持仓）----
    // 买 100@10，卖 300@12 -> 只结转 100 股，净持仓不得为负
    {
        QVector<TradeRecord> r{ buy(100, 10.0, d0), sell(300, 12.0, d1) };
        HoldingSummary s = HoldingCalc::compute("A", r, 12.0, noFee());
        eq  ("9 超卖后净持仓不为负", s.netShares, 0);
        near("9 超卖只实现100股", s.realizedPnl, 200.0);
    }

    // ---- 用例 10: 同日买入先于卖出，顺序无关性 ----
    // 同日先给卖单再给买单，规整后应先买入再卖出
    {
        QVector<TradeRecord> r{ sell(100, 12.0, d1), buy(100, 10.0, d1) };
        HoldingSummary s = HoldingCalc::compute("A", r, 12.0, noFee());
        eq  ("10 同日乱序净持仓", s.netShares, 0);
        near("10 同日乱序已实现", s.realizedPnl, 200.0);
    }

    // ---- 用例 11: 多次加仓的成本价（含买入费用）----
    // 买 1000@10 费用5.1 -> 成本基础 10005.1；买 1000@20 费用5.0 -> 20005.0
    // 合计 30010.1 / 2000 = 15.00505
    {
        QVector<TradeRecord> r{ buy(1000, 10.0, d0), buy(1000, 20.0, d1) };
        HoldingSummary s = HoldingCalc::compute("A", r, 15.005, FeeConfig());
        near("11 含费成本价", s.costPrice, 30010.1 / 2000.0, 0.002);
        eq  ("11 净持仓", s.netShares, 2000);
        near("11 浮动盈亏近似0", s.unrealizedPnl, 0.0, 0.5);
    }

    printf("\n===== 结果: %d 通过, %d 失败 =====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
