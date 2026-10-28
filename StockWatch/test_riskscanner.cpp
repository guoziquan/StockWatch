// 风险扫描器判定逻辑验证（离线，无网络）
// 编译: g++ -std=gnu++11 -I. test_riskscanner.cpp riskscanner.cpp -o t.exe -lQt5Core
#include "riskscanner.h"
#include <QCoreApplication>
#include <cstdio>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, bool ok)
{
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", name);
}

static void checkF(const char *name, bool ok, const QStringList &ls)
{
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", name);
    if (!ok) { for (const auto &s : ls) printf("        -> %s\n", qPrintable(s)); }
}

static AnnouncementItem ann(const QString &title, const QString &date = "2026-04-25")
{
    AnnouncementItem a; a.title = title; a.noticeDate = date; return a;
}

static CashFlowItem cf(const QString &date, double cash, double profit, double deduct)
{
    CashFlowItem c;
    c.reportDate = date; c.netCashOperate = cash;
    c.netProfit = profit; c.deductNetProfit = deduct;
    return c;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    printf("=== 1. 审计意见分类 ===\n");
    check("无法表示意见 -> Disclaimer",
          RiskScanner::classifyAuditOpinion(QStringLiteral("2025年年度报告（无法表示意见）")) == AuditOpinionType::Disclaimer);
    check("否定意见 -> Adverse",
          RiskScanner::classifyAuditOpinion(QStringLiteral("否定意见审计报告")) == AuditOpinionType::Adverse);
    check("保留意见 -> Qualified",
          RiskScanner::classifyAuditOpinion(QStringLiteral("保留意见审计报告")) == AuditOpinionType::Qualified);
    check("带强调事项 -> Emphasis",
          RiskScanner::classifyAuditOpinion(QStringLiteral("带强调事项段的无保留意见")) == AuditOpinionType::Emphasis);
    check("标准无保留 -> Standard",
          RiskScanner::classifyAuditOpinion(QStringLiteral("标准无保留意见审计报告")) == AuditOpinionType::Standard);
    check("空文本 -> Unknown",
          RiskScanner::classifyAuditOpinion(QString()) == AuditOpinionType::Unknown);
    check("无关文本 -> Unknown",
          RiskScanner::classifyAuditOpinion(QStringLiteral("关于回购的公告")) == AuditOpinionType::Unknown);
    // 顺序陷阱：同时含"无法表示"和"保留"时必须判为最严重的
    check("同时含无法表示+保留 -> 取最严重",
          RiskScanner::classifyAuditOpinion(QStringLiteral("保留意见改为无法表示意见")) == AuditOpinionType::Disclaimer);
    // "带强调事项的无保留意见" 必须判 Emphasis 而非 Standard
    check("带强调事项的无保留意见 -> Emphasis 而非 Standard",
          RiskScanner::classifyAuditOpinion(QStringLiteral("带强调事项段的无保留意见")) == AuditOpinionType::Emphasis);

    printf("\n=== 2. 审计意见红旗 ===\n");
    {
        QVector<CashFlowItem> v;
        CashFlowItem c = cf("2025-12-31", 1e8, 2e8, 1.8e8);
        c.auditOpinion = QStringLiteral("2026-04-25 2025年年度报告（无法表示意见）");
        v << c;
        QStringList f = RiskScanner::checkAuditOpinion(v);
        checkF("无法表示意见触发红旗", f.size() == 1 && f.first().contains("无法表示意见"), f);
    }
    {
        QVector<CashFlowItem> v;
        CashFlowItem c = cf("2025-12-31", 1e8, 2e8, 1.8e8);
        c.auditOpinion = QStringLiteral("2026-04-25 标准无保留意见审计报告");
        v << c;
        QStringList f = RiskScanner::checkAuditOpinion(v);
        checkF("标准无保留意见不触发", f.isEmpty(), f);
    }
    {
        QVector<CashFlowItem> v;
        CashFlowItem c = cf("2025-12-31", 1e8, 2e8, 1.8e8);
        c.auditOpinion = QStringLiteral("2026-04-25 带强调事项段的无保留意见");
        v << c;
        QStringList f = RiskScanner::checkAuditOpinion(v);
        checkF("带强调事项触发(需关注持续经营)", f.size() == 1, f);
    }

    printf("\n=== 3. 扣非净利润占比 ===\n");
    {
        // 净利润 10 亿，扣非 -2 亿 -> 全靠非经常性损益
        QVector<CashFlowItem> v{ cf("2026-09-30", 12e8, 10e8, -2e8) };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        checkF("扣非为负但净利润为正 -> 红旗", !f.isEmpty() && f.first().contains("非经常性损益"), f);
    }
    {
        // 净利润 10 亿，扣非 2 亿 -> 占比 20% < 50%
        QVector<CashFlowItem> v{ cf("2026-09-30", 12e8, 10e8, 2e8) };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        checkF("扣非占比20% -> 红旗", !f.isEmpty() && f.first().contains("主业盈利能力弱"), f);
    }
    {
        // 净利润 10 亿，扣非 9 亿 -> 占比 90%，健康
        QVector<CashFlowItem> v{ cf("2026-09-30", 12e8, 10e8, 9e8) };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        checkF("扣非占比90% -> 不报警", f.isEmpty(), f);
    }
    {
        // 亏损公司：净利润 -5 亿，不应因"扣非占比"报警
        QVector<CashFlowItem> v{ cf("2026-09-30", -3e8, -5e8, -6e8) };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        checkF("亏损公司不误报扣非占比", f.isEmpty(), f);
    }

    printf("\n=== 4. 经营现金流 / 净利润 覆盖 ===\n");
    {
        // 3 期均盈利，但现金流合计远低于净利润 -> 利润未转化
        QVector<CashFlowItem> v{
            cf("2026-09-30", 1e7, 5e8, 5e8),
            cf("2026-06-30", 1e7, 5e8, 5e8),
            cf("2026-03-31", 1e7, 5e8, 5e8)
        };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        bool hit = false;
        for (const auto &s : f) if (s.contains("盈利质量存疑") || s.contains("未转化为现金")) hit = true;
        checkF("现金流覆盖不足 -> 红旗", hit, f);
    }
    {
        // 现金流充裕
        QVector<CashFlowItem> v{
            cf("2026-09-30", 6e8, 5e8, 4.8e8),
            cf("2026-06-30", 6e8, 5e8, 4.8e8),
            cf("2026-03-31", 6e8, 5e8, 4.8e8)
        };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        checkF("现金流覆盖充足 -> 不报警", f.isEmpty(), f);
    }
    {
        // 亏损期不参与覆盖计算：仅 2 期盈利且覆盖良好
        QVector<CashFlowItem> v{
            cf("2026-09-30", 9e8, 5e8, 5e8),
            cf("2026-06-30", -2e8, -1e8, -1e8),   // 亏损期跳过
            cf("2026-03-31", 9e8, 5e8, 5e8)
        };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        checkF("亏损期不参与覆盖计算", f.isEmpty(), f);
    }

    printf("\n=== 5. 审计意见从公告提取 ===\n");
    {
        QVector<AnnouncementItem> a{ ann(QStringLiteral("2025年年度报告（无法表示意见）")) };
        QString op = RiskScanner::extractAuditOpinion(a);
        check("从公告提取到审计意见", !op.isEmpty() && op.contains("无法表示意见"));
    }
    {
        QVector<AnnouncementItem> a{ ann(QStringLiteral("关于回购股份的公告")) };
        check("无关公告不误提取", RiskScanner::extractAuditOpinion(a).isEmpty());
    }
    {
        QVector<AnnouncementItem> a{ ann(QStringLiteral("标准无保留意见审计报告")) };
        QString op = RiskScanner::extractAuditOpinion(a);
        check("标准意见也能识别(不产生红旗)", !op.isEmpty());
    }

    printf("\n=== 5b. 真实公告标题的误判防护 ===\n");
    {
        // 真实标题（已通过东财接口核实）：法律意见书，绝不能当作审计意见
        QVector<AnnouncementItem> a{
            ann(QStringLiteral("三七互娱:北京市天元律师事务所关于三七互娱2026年第一次临时股东会的法律意见"))
        };
        checkF("法律意见书不误提取", RiskScanner::extractAuditOpinion(a).isEmpty(),
               QStringList{RiskScanner::extractAuditOpinion(a)});
    }
    {
        // 真实标题：核查意见
        QVector<AnnouncementItem> a{
            ann(QStringLiteral("三七互娱:东方证券关于三七互娱使用闲置募集资金进行现金管理的核查意见"))
        };
        check("核查意见不误提取", RiskScanner::extractAuditOpinion(a).isEmpty());
    }
    {
        // 真实标题：内控审计报告
        QVector<AnnouncementItem> a{
            ann(QStringLiteral("贵州茅台:贵州茅台2025年度内部控制审计报告"))
        };
        check("内控审计报告不误提取", RiskScanner::extractAuditOpinion(a).isEmpty());
    }
    {
        // 真实标题：年度审计报告（标题不含意见类型，无法判定）
        QVector<AnnouncementItem> a{
            ann(QStringLiteral("贵州茅台:贵州茅台2025年度审计报告"))
        };
        check("无意见类型的审计报告 -> 提取为空(不臆测)", RiskScanner::extractAuditOpinion(a).isEmpty());
    }
    {
        // 真实标题：审计委员会履职报告
        QVector<AnnouncementItem> a{
            ann(QStringLiteral("贵州茅台:贵州茅台2025年度董事会审计委员会履职情况报告"))
        };
        check("审计委员会报告不误提取", RiskScanner::extractAuditOpinion(a).isEmpty());
    }
    {
        // 若确实出现带意见类型的标题，应能正确提取
        QVector<AnnouncementItem> a{
            ann(QStringLiteral("某某公司:2025年年度报告（无法表示意见）"), "2026-04-30")
        };
        QString op = RiskScanner::extractAuditOpinion(a);
        check("带意见类型的标题正确提取", op.contains("无法表示意见") && op.contains("2026-04-30"));
    }

    printf("\n=== 5c. 数据完整性提示 ===\n");
    {
        // 无审计意见文本时，必须报数据缺口
        RiskResult r = RiskScanner::analyze("000001", "测试",
            QVector<AnnouncementItem>(),
            QVector<CashFlowItem>{ cf("2026-09-30", 9e8, 10e8, 9.5e8) });
        check("审计意见缺失 -> auditChecked=false", !r.auditChecked);
        check("审计意见缺失 -> dataGaps 有提示", !r.dataGaps.isEmpty());
    }
    {
        // 有审计意见文本时，不再报该缺口
        QVector<CashFlowItem> v;
        CashFlowItem c = cf("2025-12-31", 9e8, 10e8, 9.5e8);
        c.auditOpinion = QStringLiteral("2026-04-25 标准无保留意见审计报告");
        v << c;
        RiskResult r = RiskScanner::analyze("000001", "测试", QVector<AnnouncementItem>(), v);
        check("有审计意见 -> auditChecked=true", r.auditChecked);
    }
    {
        // 财务数据为空 -> 报缺口
        RiskResult r = RiskScanner::analyze("000009", "空",
            QVector<AnnouncementItem>(), QVector<CashFlowItem>());
        bool hasFinancialGap = false;
        for (const auto &g : r.dataGaps) if (g.contains("财务数据未获取")) hasFinancialGap = true;
        check("财务数据缺失 -> dataGaps 有提示", hasFinancialGap);
    }

    printf("\n=== 6. analyze 综合判定 ===\n");
    {
        // 完全干净：无异动公告 + 财务健康
        RiskResult r = RiskScanner::analyze("000001", "平安银行",
            QVector<AnnouncementItem>(),
            QVector<CashFlowItem>{ cf("2026-09-30", 9e8, 10e8, 9.5e8) });
        check("健康公司 hasRisk=false", !r.hasRisk);
    }
    {
        // 仅有审计意见红旗
        QVector<CashFlowItem> v;
        CashFlowItem c = cf("2025-12-31", 9e8, 10e8, 9.5e8);
        c.auditOpinion = QStringLiteral("2026-04-25 保留意见审计报告");
        v << c;
        RiskResult r = RiskScanner::analyze("300001", "某公司", QVector<AnnouncementItem>(), v);
        check("保留意见 -> hasRisk=true", r.hasRisk);
        check("保留意见进入 redFlags", !r.redFlags.isEmpty() && r.redFlags.first().contains("保留意见"));
    }
    {
        // 公告风险 + 财务健康
        RiskResult r = RiskScanner::analyze("300002", "某公司",
            QVector<AnnouncementItem>{ ann(QStringLiteral("关于收到证监会立案调查通知的公告")) },
            QVector<CashFlowItem>{ cf("2026-09-30", 9e8, 10e8, 9.5e8) });
        check("立案调查 -> hasRisk=true", r.hasRisk);
        check("立案调查进入 hits", !r.hits.isEmpty());
    }
    {
        // 空数据不崩溃
        RiskResult r = RiskScanner::analyze("000003", "空数据",
            QVector<AnnouncementItem>(), QVector<CashFlowItem>());
        check("空数据安全返回 hasRisk=false", !r.hasRisk);
    }

    printf("\n=== 7. 边界：异常数值 ===\n");
    {
        QVector<CashFlowItem> v{ cf("2026-09-30", 0, 0, 0) };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        checkF("全零数据不误报", f.isEmpty(), f);
    }
    {
        // 净利润 0 但扣非 -1 亿
        QVector<CashFlowItem> v{ cf("2026-09-30", 1e8, 0, -1e8) };
        QStringList f = RiskScanner::checkEarningsQuality(v);
        checkF("净利润为0时不判扣非占比", f.isEmpty(), f);
    }

    printf("\n=== 8. 公告关键词误报防护（真实数据）===\n");
    {
        // 真实场景：正常公司的例行公告 + 「资金占用」栏目分类
        // 这些公司全部是健康公司，绝不能判为风险
        QVector<AnnouncementItem> a;
        AnnouncementItem i1 = ann(QStringLiteral("恺英网络:2026年半年度非经营性资金占用及其他关联资金往来情况汇总表"));
        i1.categories << QStringLiteral("资金占用");
        AnnouncementItem i2 = ann(QStringLiteral("恺英网络:国浩律师(上海)事务所关于恺英网络2026年第一次临时股东会法律意见书"));
        i2.categories << QStringLiteral("法律意见书");
        AnnouncementItem i3 = ann(QStringLiteral("恺英网络:董事会薪酬与考核委员会关于股票期权激励计划调整事项的核查意见"));
        i3.categories << QStringLiteral("保荐/核查意见");
        AnnouncementItem i4 = ann(QStringLiteral("恺英网络:2026年半年度报告"));
        i4.categories << QStringLiteral("半年度报告全文");
        AnnouncementItem i5 = ann(QStringLiteral("恺英网络:关于会计政策变更的公告"));
        i5.categories << QStringLiteral("其他");
        a << i1 << i2 << i3 << i4 << i5;

        QStringList hits = RiskScanner::matchAnnouncementKeywords(a);
        checkF("健康公司的例行公告不误报", hits.isEmpty(), hits);
    }
    {
        // 「资金占用」分类但标题是例行汇总表 -> 不报
        QVector<AnnouncementItem> a;
        AnnouncementItem x = ann(QStringLiteral("某某公司:2026年半年度非经营性资金占用及其他关联资金往来情况汇总表"));
        x.categories << QStringLiteral("资金占用");
        a << x;
        checkF("例行资金占用汇总表不误报", RiskScanner::matchAnnouncementKeywords(a).isEmpty(),
               RiskScanner::matchAnnouncementKeywords(a));
    }
    {
        // 真正的风险：标题明确写"立案调查"
        QVector<AnnouncementItem> a{ ann(QStringLiteral("某某公司:关于收到中国证监会立案调查告知书的公告")) };
        QStringList hits = RiskScanner::matchAnnouncementKeywords(a);
        checkF("立案调查标题仍能命中", !hits.isEmpty(), hits);
    }
    {
        // 真正的风险：问询函
        QVector<AnnouncementItem> a{ ann(QStringLiteral("某某公司:关于收到深圳证券交易所问询函的公告")) };
        QStringList hits = RiskScanner::matchAnnouncementKeywords(a);
        checkF("问询函标题仍能命中", !hits.isEmpty(), hits);
    }
    {
        // 违规担保
        QVector<AnnouncementItem> a{ ann(QStringLiteral("某某公司:关于违规担保事项的进展公告")) };
        checkF("违规担保标题仍能命中", !RiskScanner::matchAnnouncementKeywords(a).isEmpty(),
               RiskScanner::matchAnnouncementKeywords(a));
    }
    {
        // 旧版过宽关键词「违规」不应再误伤普通合规公告
        QVector<AnnouncementItem> a{ ann(QStringLiteral("某某公司:关于不存在违规情形的说明公告")) };
        checkF("「不存在违规情形」不误报", RiskScanner::matchAnnouncementKeywords(a).isEmpty(),
               RiskScanner::matchAnnouncementKeywords(a));
    }

    printf("\n=== 9. 营收下滑判定 ===\n");
    {
        // 营收同比 -70% -> 断崖式下滑
        QVector<CashFlowItem> v;
        CashFlowItem c = cf("2026-06-30", -1.7e8, -0.67e8, -0.7e8);
        c.totalOperateIncome = 0.57e8;
        c.incomeYoY = -70.0;
        v << c;
        QStringList f = RiskScanner::checkCashFlowRedFlags(v);
        bool hit = false;
        for (const auto &s : f) if (s.contains("主业大幅萎缩")) hit = true;
        checkF("营收同比-70% -> 断崖红旗", hit, f);
    }
    {
        // 营收同比 -10% -> 不触发断崖
        QVector<CashFlowItem> v;
        CashFlowItem c = cf("2026-06-30", 5e7, 5e7, 5e7);
        c.totalOperateIncome = 10e8;
        c.incomeYoY = -10.0;
        v << c;
        QStringList f = RiskScanner::checkCashFlowRedFlags(v);
        bool hit = false;
        for (const auto &s : f) if (s.contains("主业大幅萎缩")) hit = true;
        checkF("营收同比-10% 不报断崖", !hit, f);
    }
    {
        // 连续3期营收小幅下滑(-8%) -> 属正常波动，不应报警
        QVector<CashFlowItem> v;
        for (int i = 0; i < 3; ++i) {
            CashFlowItem c = cf(QStringLiteral("2026-%1-30").arg(9 - i * 3, 2, 10, QChar('0')),
                                5e7, 5e7, 5e7);
            c.totalOperateIncome = 10e8;
            c.incomeYoY = -8.0;
            v << c;
        }
        QStringList f = RiskScanner::checkCashFlowRedFlags(v);
        bool hit = false;
        for (const auto &s : f) if (s.contains("连续下滑")) hit = true;
        checkF("营收连续小幅下滑(-8%) 不报警(正常波动)", !hit, f);
    }
    {
        // 连续3期大幅下滑(最新-30%) -> 报警
        QVector<CashFlowItem> v;
        double yoys[] = { -30.0, -25.0, -22.0 };
        for (int i = 0; i < 3; ++i) {
            CashFlowItem c = cf(QStringLiteral("2026-%1-30").arg(9 - i * 3, 2, 10, QChar('0')),
                                5e7, 5e7, 5e7);
            c.totalOperateIncome = 10e8;
            c.incomeYoY = yoys[i];
            v << c;
        }
        QStringList f = RiskScanner::checkCashFlowRedFlags(v);
        bool hit = false;
        for (const auto &s : f) if (s.contains("连续下滑")) hit = true;
        checkF("营收连续大幅下滑(-30%) -> 报警", hit, f);
    }
    {
        // 营收增长 -> 不应报下滑
        QVector<CashFlowItem> v;
        CashFlowItem c = cf("2026-06-30", 5e7, 5e7, 5e7);
        c.totalOperateIncome = 10e8;
        c.incomeYoY = 30.0;
        v << c;
        QStringList f = RiskScanner::checkCashFlowRedFlags(v);
        bool hit = false;
        for (const auto &s : f) if (s.contains("萎缩") || s.contains("连续下滑")) hit = true;
        checkF("营收增长不报下滑", !hit, f);
    }

    printf("\n===== 结果: %d 通过, %d 失败 =====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
