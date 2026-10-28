// 板块联动与相对强度验证（离线）
#include "sectordata.h"
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
    printf("%s %-46s got=%8.3f want=%8.3f\n", ok ? "[PASS]" : "[FAIL]", name, got, want);
}

static SectorItem mk(const QString &code, const QString &name, double pct)
{
    SectorItem s;
    s.code = code; s.name = name; s.changePct = pct;
    s.kind = SectorCalc::kindOf(code, name);
    return s;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    printf("=== 1. 板块类型识别 ===\n");
    check("游戏Ⅱ -> 行业",
          SectorCalc::kindOf("BK1046", QStringLiteral("游戏Ⅱ")) == SectorKind::Industry);
    check("游戏Ⅲ -> 行业",
          SectorCalc::kindOf("BK1301", QStringLiteral("游戏Ⅲ")) == SectorKind::Industry);
    check("传媒 -> 行业(白名单)",
          SectorCalc::kindOf("BK0486", QStringLiteral("传媒")) == SectorKind::Industry);
    check("电力 -> 行业(白名单)",
          SectorCalc::kindOf("BK0428", QStringLiteral("电力")) == SectorKind::Industry);
    check("安徽板块 -> 地域",
          SectorCalc::kindOf("BK0149", QStringLiteral("安徽板块")) == SectorKind::Region);
    check("网络游戏 -> 概念",
          SectorCalc::kindOf("BK0509", QStringLiteral("网络游戏")) == SectorKind::Concept);

    printf("\n=== 2. 主板块选择：优先行业 ===\n");
    {
        // 三七互娱真实板块数据
        QVector<SectorItem> sectors{
            mk("BK0486", QStringLiteral("传媒"),    2.35),
            mk("BK1301", QStringLiteral("游戏Ⅲ"),   2.62),
            mk("BK1046", QStringLiteral("游戏Ⅱ"),   2.62),
            mk("BK0149", QStringLiteral("安徽板块"), 1.63),
            mk("BK0509", QStringLiteral("网络游戏"), 2.31),
            mk("BK0706", QStringLiteral("脑机接口"), 2.53)
        };
        SectorItem p = SectorCalc::pickPrimary(sectors);
        check("选中行业板块", p.kind == SectorKind::Industry);
        check("选中游戏Ⅱ或游戏Ⅲ", p.name == QStringLiteral("游戏Ⅱ")
                                 || p.name == QStringLiteral("游戏Ⅲ"));
        near("主板块涨幅 2.62", p.changePct, 2.62);
    }
    {
        // 只有概念板块时，优先"概念"结尾
        QVector<SectorItem> sectors{
            mk("BK0149", QStringLiteral("安徽板块"), 1.63),
            mk("BK0509", QStringLiteral("网络游戏"), 2.31),
            mk("BK1009", QStringLiteral("元宇宙概念"), 1.86)
        };
        SectorItem p = SectorCalc::pickPrimary(sectors);
        check("优先选'概念'结尾的", p.name == QStringLiteral("元宇宙概念"));
    }
    {
        // 空列表
        SectorItem p = SectorCalc::pickPrimary(QVector<SectorItem>());
        check("空列表返回无效", !p.valid());
    }
    {
        // 只有地域板块时不应选地域为主板块
        QVector<SectorItem> sectors{ mk("BK0149", QStringLiteral("安徽板块"), 1.63) };
        SectorItem p = SectorCalc::pickPrimary(sectors);
        check("仅有地域板块时不选它", !p.valid());
    }

    printf("\n=== 3. 相对强度等级 ===\n");
    check("+5.0 -> 领涨", SectorCalc::levelOf(5.0) == StrengthLevel::Leader);
    check("+3.0 -> 领涨(边界)", SectorCalc::levelOf(3.0) == StrengthLevel::Leader);
    check("+2.9 -> 略强", SectorCalc::levelOf(2.9) == StrengthLevel::SlightlyUp);
    check("+1.0 -> 略强(边界)", SectorCalc::levelOf(1.0) == StrengthLevel::SlightlyUp);
    check("+0.5 -> 同步", SectorCalc::levelOf(0.5) == StrengthLevel::InLine);
    check("0.0 -> 同步", SectorCalc::levelOf(0.0) == StrengthLevel::InLine);
    check("-0.5 -> 同步", SectorCalc::levelOf(-0.5) == StrengthLevel::InLine);
    check("-2.0 -> 略弱", SectorCalc::levelOf(-2.0) == StrengthLevel::SlightlyDown);
    check("-3.0 -> 拖累(边界)", SectorCalc::levelOf(-3.0) == StrengthLevel::Laggard);
    check("-5.0 -> 拖累", SectorCalc::levelOf(-5.0) == StrengthLevel::Laggard);

    printf("\n=== 4. 组装联动结果（真实数据）===\n");
    {
        // 三七互娱 个股+3.12%，游戏Ⅱ +2.62% -> 领先 0.5 -> 同步
        QVector<SectorItem> sectors{
            mk("BK1046", QStringLiteral("游戏Ⅱ"),   2.62),
            mk("BK0486", QStringLiteral("传媒"),    2.35),
            mk("BK0149", QStringLiteral("安徽板块"), 1.63)
        };
        SectorLink l = SectorCalc::build("002555", QStringLiteral("三七互娱"), 3.12, sectors);
        check("有主板块", l.hasPrimary);
        check("主板块为游戏Ⅱ", l.primary.name == QStringLiteral("游戏Ⅱ"));
        near("相对强度 = 0.50", l.relStrength, 0.50);
        check("等级=同步", l.level == StrengthLevel::InLine);
        check("有结论文字", !l.verdict.isEmpty());
    }
    {
        // 板块+5%，个股+1% -> 落后 4 -> 拖累
        QVector<SectorItem> sectors{ mk("BK1046", QStringLiteral("游戏Ⅱ"), 5.0) };
        SectorLink l = SectorCalc::build("002555", QStringLiteral("某股"), 1.0, sectors);
        near("相对强度 = -4.0", l.relStrength, -4.0);
        check("等级=拖累", l.level == StrengthLevel::Laggard);
        check("结论含'明显弱于'", l.verdict.contains(QStringLiteral("明显弱于")));
    }
    {
        // 板块+0.5%，个股+6% -> 领先 5.5 -> 领涨
        QVector<SectorItem> sectors{ mk("BK1046", QStringLiteral("游戏Ⅱ"), 0.5) };
        SectorLink l = SectorCalc::build("002555", QStringLiteral("某股"), 6.0, sectors);
        near("相对强度 = 5.5", l.relStrength, 5.5);
        check("等级=领涨", l.level == StrengthLevel::Leader);
        check("结论含'独立强势'", l.verdict.contains(QStringLiteral("独立强势")));
    }
    {
        // 无板块数据
        SectorLink l = SectorCalc::build("002555", QStringLiteral("某股"), 3.0,
                                          QVector<SectorItem>());
        check("无数据时 hasPrimary=false", !l.hasPrimary);
        check("无数据时等级=NoData", l.level == StrengthLevel::NoData);
        check("无数据时结论非空", !l.verdict.isEmpty());
    }

    printf("\n=== 5. 排序 ===\n");
    {
        QVector<SectorItem> sectors{
            mk("A", QStringLiteral("跌的"), -1.0),
            mk("B", QStringLiteral("涨的多"), 3.0),
            mk("C", QStringLiteral("涨的少"), 0.5)
        };
        QVector<SectorItem> sorted = SectorCalc::sortedByPct(sectors);
        check("降序: 第一是3.0", sorted[0].changePct == 3.0);
        check("降序: 第二是0.5", sorted[1].changePct == 0.5);
        check("降序: 第三是-1.0", sorted[2].changePct == -1.0);
    }

    printf("\n=== 6. 等级/类型中文名 ===\n");
    check("Leader -> 领涨", SectorCalc::levelName(StrengthLevel::Leader) == QStringLiteral("领涨"));
    check("Industry -> 行业", SectorCalc::kindName(SectorKind::Industry) == QStringLiteral("行业"));
    check("Region -> 地域", SectorCalc::kindName(SectorKind::Region) == QStringLiteral("地域"));

    printf("\n=== 7. 边界 ===\n");
    {
        // 板块涨幅恰好等于个股
        QVector<SectorItem> sectors{ mk("BK1046", QStringLiteral("游戏Ⅱ"), 2.0) };
        SectorLink l = SectorCalc::build("002555", "X", 2.0, sectors);
        near("相对强度 = 0", l.relStrength, 0.0);
        check("等级=同步", l.level == StrengthLevel::InLine);
    }

    printf("\n===== 结果: %d 通过, %d 失败 =====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
