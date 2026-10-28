// 涨跌停规则验证（离线）
#include "pricelimit.h"
#include <QCoreApplication>
#include <cstdio>
#include <cmath>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, bool ok)
{
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", name);
}

static void near(const char *name, double got, double want, double eps = 0.005)
{
    const bool ok = std::fabs(got - want) <= eps;
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %-46s got=%9.3f want=%9.3f\n", ok ? "[PASS]" : "[FAIL]", name, got, want);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    printf("=== 1. 板块识别 ===\n");
    check("600519 -> 沪市主板",
          PriceLimitCalc::boardOf("600519") == BoardType::MainSh);
    check("603444 -> 沪市主板",
          PriceLimitCalc::boardOf("603444") == BoardType::MainSh);
    check("002555 -> 深市主板",
          PriceLimitCalc::boardOf("002555") == BoardType::MainSz);
    check("000001 -> 深市主板",
          PriceLimitCalc::boardOf("000001") == BoardType::MainSz);
    check("300533 -> 创业板",
          PriceLimitCalc::boardOf("300533") == BoardType::ChiNext);
    check("301117 -> 创业板",
          PriceLimitCalc::boardOf("301117") == BoardType::ChiNext);
    check("688981 -> 科创板",
          PriceLimitCalc::boardOf("688981") == BoardType::StarMarket);
    check("430047 -> 北交所",
          PriceLimitCalc::boardOf("430047") == BoardType::BSE);
    check("830799 -> 北交所",
          PriceLimitCalc::boardOf("830799") == BoardType::BSE);

    printf("\n=== 2. 风险状态识别 ===\n");
    check("ST佳缘 -> ST",
          PriceLimitCalc::riskStatusOf(QStringLiteral("ST佳缘")) == RiskStatus::ST);
    check("*ST海投 -> StarST",
          PriceLimitCalc::riskStatusOf(QStringLiteral("*ST海投")) == RiskStatus::StarST);
    check("三七互娱 -> Normal",
          PriceLimitCalc::riskStatusOf(QStringLiteral("三七互娱")) == RiskStatus::Normal);
    check("带空格 ST 佳缘 -> ST",
          PriceLimitCalc::riskStatusOf(QStringLiteral("ST 佳缘")) == RiskStatus::ST);

    printf("\n=== 3. 涨跌幅限制百分比 ===\n");
    near("主板正常 = 10%",  PriceLimitCalc::limitPctOf(BoardType::MainSh, RiskStatus::Normal), 10.0);
    near("主板ST = 5%",     PriceLimitCalc::limitPctOf(BoardType::MainSh, RiskStatus::ST), 5.0);
    near("主板*ST = 5%",    PriceLimitCalc::limitPctOf(BoardType::MainSz, RiskStatus::StarST), 5.0);
    near("创业板正常 = 20%", PriceLimitCalc::limitPctOf(BoardType::ChiNext, RiskStatus::Normal), 20.0);
    near("创业板ST = 20%(关键)", PriceLimitCalc::limitPctOf(BoardType::ChiNext, RiskStatus::ST), 20.0);
    near("科创板正常 = 20%", PriceLimitCalc::limitPctOf(BoardType::StarMarket, RiskStatus::Normal), 20.0);
    near("科创板ST = 20%",   PriceLimitCalc::limitPctOf(BoardType::StarMarket, RiskStatus::ST), 20.0);
    near("北交所 = 30%",     PriceLimitCalc::limitPctOf(BoardType::BSE, RiskStatus::Normal), 30.0);

    printf("\n=== 4. 真实案例：ST佳缘 301117 ===\n");
    {
        // 实测行情：昨收 12.00，腾讯显示涨停 14.40 / 跌停 9.60
        PriceLimit pl = PriceLimitCalc::compute("301117", QStringLiteral("ST佳缘"), 12.00);
        check("有效", pl.valid);
        near("涨停价 = 14.40", pl.upPrice, 14.40);
        near("跌停价 = 9.60", pl.downPrice, 9.60);
        near("限制 = 20%", pl.limitPct, 20.0);
        check("板块=创业板", pl.board == BoardType::ChiNext);
        check("风险=ST", pl.risk == RiskStatus::ST);
    }

    printf("\n=== 5. 主板 ST 对照（应为 ±5%）===\n");
    {
        PriceLimit pl = PriceLimitCalc::compute("600001", QStringLiteral("ST某某"), 10.00);
        near("涨停 = 10.50", pl.upPrice, 10.50);
        near("跌停 = 9.50",  pl.downPrice, 9.50);
        near("限制 = 5%",    pl.limitPct, 5.0);
    }

    printf("\n=== 6. 主板正常股 ===\n");
    {
        PriceLimit pl = PriceLimitCalc::compute("002555", QStringLiteral("三七互娱"), 16.15);
        near("涨停 = 17.77", pl.upPrice, 17.77);   // 16.15*1.1 = 17.765 -> 17.77
        near("跌停 = 14.54", pl.downPrice, 14.54); // 16.15*0.9 = 14.535 -> 14.54
    }

    printf("\n=== 7. 涨停/跌停判定 ===\n");
    {
        PriceLimit pl = PriceLimitCalc::compute("301117", QStringLiteral("ST佳缘"), 12.00);
        check("14.40 判定为涨停", PriceLimitCalc::isLimitUp(14.40, pl));
        check("14.39 非涨停",     !PriceLimitCalc::isLimitUp(14.39, pl));
        check("9.60 判定为跌停",  PriceLimitCalc::isLimitDown(9.60, pl));
        check("9.61 非跌停",      !PriceLimitCalc::isLimitDown(9.61, pl));
        check("12.75 既非涨停也非跌停",
              !PriceLimitCalc::isLimitUp(12.75, pl) && !PriceLimitCalc::isLimitDown(12.75, pl));
    }

    printf("\n=== 8. 距离涨跌停空间 ===\n");
    {
        PriceLimit pl = PriceLimitCalc::compute("301117", QStringLiteral("ST佳缘"), 12.00);
        // 现价 12.75，涨停 14.40 -> (14.40-12.75)/12.75 = 12.94%
        near("距涨停 12.94%", PriceLimitCalc::distToUpPct(12.75, pl), 12.94, 0.02);
        // 距跌停 (12.75-9.60)/12.75 = 24.71%
        near("距跌停 24.71%", PriceLimitCalc::distToDownPct(12.75, pl), 24.71, 0.02);
    }

    printf("\n=== 9. 边界：无效昨收 ===\n");
    {
        PriceLimit pl = PriceLimitCalc::compute("301117", QStringLiteral("ST佳缘"), 0.0);
        check("昨收0 -> 无效", !pl.valid);
        check("无效时涨停价为0", pl.upPrice == 0.0);
    }

    printf("\n=== 10. 四舍五入边界 ===\n");
    {
        // 12.005 * 0.95 = 11.40475 -> 11.40
        PriceLimit pl = PriceLimitCalc::compute("600001", QStringLiteral("ST某"), 12.005);
        near("跌停四舍五入正确", pl.downPrice, 11.40, 0.011);
    }

    printf("\n=== 11. 说明文字 ===\n");
    {
        PriceLimit pl = PriceLimitCalc::compute("301117", QStringLiteral("ST佳缘"), 12.00);
        printf("  %s\n", qPrintable(pl.explanation));
        check("说明含创业板", pl.explanation.contains(QStringLiteral("创业板")));
        check("说明含20%", pl.explanation.contains(QStringLiteral("20%")));
    }

    printf("\n===== 结果: %d 通过, %d 失败 =====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
