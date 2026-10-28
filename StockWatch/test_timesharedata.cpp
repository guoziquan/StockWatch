// 分时数据解析与槽位映射验证（离线）
#include "timesharedata.h"
#include <QCoreApplication>
#include <cstdio>
#include <cmath>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, bool ok)
{
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", name);
}

static void near(const char *name, double got, double want, double eps = 0.001)
{
    const bool ok = std::fabs(got - want) <= eps;
    if (ok) ++g_pass; else ++g_fail;
    printf("%s %-44s got=%9.3f want=%9.3f\n", ok ? "[PASS]" : "[FAIL]", name, got, want);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    printf("=== 1. 槽位映射（关键：午休不留空隙）===\n");
    check("09:30 -> slot 0", TimeShareData::slotOf("2026-10-28 09:30") == 0);
    check("09:31 -> slot 1", TimeShareData::slotOf("2026-10-28 09:31") == 1);
    check("10:30 -> slot 60", TimeShareData::slotOf("2026-10-28 10:30") == 60);
    check("11:30 -> slot 120", TimeShareData::slotOf("2026-10-28 11:30") == 120);
    check("13:00 -> slot 120(与11:30同格)", TimeShareData::slotOf("2026-10-28 13:00") == 120);
    check("13:01 -> slot 121", TimeShareData::slotOf("2026-10-28 13:01") == 121);
    check("14:00 -> slot 180", TimeShareData::slotOf("2026-10-28 14:00") == 180);
    check("15:00 -> slot 240", TimeShareData::slotOf("2026-10-28 15:00") == 240);

    printf("\n=== 2. 非交易时段返回 -1 ===\n");
    check("09:29 -> -1", TimeShareData::slotOf("2026-10-28 09:29") == -1);
    check("12:00(午休) -> -1", TimeShareData::slotOf("2026-10-28 12:00") == -1);
    check("12:59(午休) -> -1", TimeShareData::slotOf("2026-10-28 12:59") == -1);
    check("15:01 -> -1", TimeShareData::slotOf("2026-10-28 15:01") == -1);

    printf("\n=== 3. 槽位标签还原 ===\n");
    check("slot 0 -> 09:30", TimeShareData::slotLabel(0) == "09:30");
    check("slot 60 -> 10:30", TimeShareData::slotLabel(60) == "10:30");
    check("slot 120 -> 11:30", TimeShareData::slotLabel(120) == "11:30");
    check("slot 180 -> 14:00", TimeShareData::slotLabel(180) == "14:00");
    check("slot 240 -> 15:00", TimeShareData::slotLabel(240) == "15:00");

    printf("\n=== 4. 解析真实格式数据 ===\n");
    {
        QVector<QString> lines{
            QStringLiteral("2026-10-28 09:30,16.11,16.11,16.11,16.11,1742,2806362.00,16.110"),
            QStringLiteral("2026-10-28 09:31,16.14,16.25,16.26,16.10,3562,5763456.00,16.159"),
            QStringLiteral("2026-10-28 09:32,16.24,16.24,16.28,16.21,4145,6737973.00,16.201")
        };
        TimeShareData d = TimeShare::parse("002555", QStringLiteral("三七互娱"), 16.10, lines);
        check("解析有效", d.valid);
        check("点数 = 3", d.points.size() == 3);
        check("日期 = 2026-10-28", d.date == "2026-10-28");
        check("时间取 HH:mm", d.points[0].time == "09:30");
        near("第1点价格", d.points[0].price, 16.11);
        near("第2点价格", d.points[1].price, 16.25);
        near("第2点均价", d.points[1].avgPrice, 16.159);
        near("第2点成交量", d.points[1].volume, 3562);
        near("昨收", d.prevClose, 16.10);
    }

    printf("\n=== 5. 累计高低价 ===\n");
    {
        QVector<QString> lines{
            QStringLiteral("2026-10-28 09:30,16.00,16.10,16.20,15.90,100,1000,16.00"),
            QStringLiteral("2026-10-28 09:31,16.10,16.30,16.50,16.00,200,2000,16.10"),
            QStringLiteral("2026-10-28 09:32,16.30,16.20,16.35,15.80,300,3000,16.12")
        };
        TimeShareData d = TimeShare::parse("002555", "T", 16.00, lines);
        near("累计最高 = 16.50", d.points[2].high, 16.50);
        near("累计最低 = 15.80", d.points[2].low, 15.80);
    }

    printf("\n=== 6. 均价缺失时自行推算 ===\n");
    {
        QVector<QString> lines{
            QStringLiteral("2026-10-28 09:30,16.00,16.00,16.00,16.00,1000,16000,0"),
            QStringLiteral("2026-10-28 09:31,16.10,16.10,16.10,16.10,1000,16100,0")
        };
        TimeShareData d = TimeShare::parse("002555", "T", 16.00, lines);
        // 累计额 32100 / 累计量 2000 = 16.05
        near("推算均价 = 16.05", d.points[1].avgPrice, 16.05, 0.01);
    }

    printf("\n=== 7. 异常数据容错 ===\n");
    {
        QVector<QString> lines{
            QStringLiteral(""),                                     // 空行
            QStringLiteral("bad"),                                  // 字段不足
            QStringLiteral("2026-10-28 09:30,16.00,0,0,0,0,0,0"),   // 价格为0
            QStringLiteral("2026-10-28 09:31,16.00,16.10,16.10,16.00,100,1000,16.05")
        };
        TimeShareData d = TimeShare::parse("002555", "T", 16.00, lines);
        check("只保留有效点", d.points.size() == 1);
        check("有效点时间正确", d.points[0].time == "09:31");
    }
    {
        TimeShareData d = TimeShare::parse("002555", "T", 0.0, QVector<QString>());
        check("空数据 invalid", !d.valid);
        check("空数据 empty", d.empty());
    }
    {
        QVector<QString> lines{
            QStringLiteral("2026-10-28 09:30,16.00,16.10,16.10,16.00,100,1000,16.05")
        };
        TimeShareData d = TimeShare::parse("002555", "T", 0.0, lines);
        check("昨收为0时 invalid", !d.valid);
    }

    printf("\n=== 8. 涨跌幅计算 ===\n");
    near("16.10 -> 17.71 = +10%", TimeShare::chgPct(17.71, 16.10), 10.0, 0.01);
    near("16.10 -> 14.49 = -10%", TimeShare::chgPct(14.49, 16.10), -10.0, 0.01);
    near("昨收0时返回0", TimeShare::chgPct(10, 0), 0.0);

    printf("\n=== 9. last() 安全 ===\n");
    {
        TimeShareData empty;
        const MinutePoint &lp = empty.last();
        check("空数据 last() 不崩溃", !lp.valid());

        QVector<QString> lines{
            QStringLiteral("2026-10-28 09:30,16.00,16.10,16.10,16.00,100,1000,16.05")
        };
        TimeShareData d = TimeShare::parse("002555", "T", 16.00, lines);
        near("last() 返回最后一点", d.last().price, 16.10);
    }

    printf("\n=== 10. 全天 240 格完整性 ===\n");
    {
        check("TOTAL_SLOTS = 240", TimeShareData::TOTAL_SLOTS == 240);
        // 09:30 到 11:30 共 121 个时刻(含端点)，午后 121 个，重叠 11:30/13:00 槽位
        int amCount = 0, pmCount = 0;
        for (int h = 9; h <= 11; ++h)
            for (int m = 0; m < 60; ++m) {
                QString tm = QStringLiteral("%1:%2").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'));
                if (TimeShareData::slotOf("2026-10-28 " + tm) >= 0) ++amCount;
            }
        for (int h = 13; h <= 15; ++h)
            for (int m = 0; m < 60; ++m) {
                QString tm = QStringLiteral("%1:%2").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'));
                if (TimeShareData::slotOf("2026-10-28 " + tm) >= 0) ++pmCount;
            }
        printf("     上午有效分钟数=%d, 下午有效分钟数=%d\n", amCount, pmCount);
        check("上午 121 个时刻", amCount == 121);
        check("下午 121 个时刻", pmCount == 121);
    }

    printf("\n=== 11. 腾讯分时格式（累计量转每分钟）===\n");
    {
        // 真实格式: "0930 16.11 1742 2806362.00"
        //           时间 价格 累计量(手) 累计额(元)
        QVector<QString> lines{
            QStringLiteral("0930 16.11 1742 2806362.00"),
            QStringLiteral("0931 16.25 5304 8570625.94"),
            QStringLiteral("0932 16.24 9448 15306980.94")
        };
        TimeShareData d = TimeShare::parseTencent("002555", QStringLiteral("三七互娱"),
                                                   16.10, "20261028", lines);
        check("腾讯格式解析有效", d.valid);
        check("点数 = 3", d.points.size() == 3);
        check("时间补冒号 -> 09:30", d.points[0].time == "09:30");
        near("第1点价格", d.points[0].price, 16.11);
        // 第1点累计量 1742 -> 每分钟 1742
        near("第1点分钟量 = 1742", d.points[0].volume, 1742);
        // 第2点累计 5304 - 1742 = 3562
        near("第2点分钟量 = 3562", d.points[1].volume, 3562);
        // 第3点累计 9448 - 5304 = 4144
        near("第3点分钟量 = 4144", d.points[2].volume, 4144);
        // 均价 = 2806362 / (1742*100) = 16.1100
        near("第1点均价 16.110", d.points[0].avgPrice, 16.110, 0.002);
        // 第3点均价 = 15306980.94 / (9448*100) = 16.201
        near("第3点均价 16.201", d.points[2].avgPrice, 16.201, 0.002);
        check("日期正确", d.date == "20261028");
    }
    {
        // 累计量不增长时（停牌分钟）不应出现负量
        QVector<QString> lines{
            QStringLiteral("0930 16.11 1742 2806362.00"),
            QStringLiteral("0931 16.11 1742 2806362.00")
        };
        TimeShareData d = TimeShare::parseTencent("002555", "T", 16.10, "20261028", lines);
        near("停牌分钟量为0(不为负)", d.points[1].volume, 0.0, 0.001);
    }
    {
        TimeShareData d = TimeShare::parseTencent("002555", "T", 0.0, "20261028",
                                                   QVector<QString>());
        check("腾讯空数据 invalid", !d.valid);
    }

    printf("\n===== 结果: %d 通过, %d 失败 =====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
