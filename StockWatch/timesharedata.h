#ifndef TIMESHAREDATA_H
#define TIMESHAREDATA_H

#include <QString>
#include <QVector>

// ============================================================================
// 分时图数据模型
//
// 与 K 线不同，分时图的横轴是"固定时间刻度"（09:30~11:30, 13:00~15:00），
// 而不是"数据条数"。因此即使上午数据只有 121 点，图上也必须把全天
// 240 个位置都预留出来，右侧留白 —— 这样用户能直观看出"现在走到一天的哪个位置"。
// ============================================================================

// ---------- 单个分时点 ----------
struct MinutePoint
{
    QString time;         // "HH:mm"
    double  price   = 0;  // 当前价
    double  avgPrice= 0;  // 均价（成交额/成交量）
    double  volume  = 0;  // 该分钟成交量(手)
    double  amount  = 0;  // 该分钟成交额(元)
    double  high    = 0;  // 当日累计最高
    double  low     = 0;  // 当日累计最低
    bool    valid() const { return price > 0; }
};

// ---------- 全天分时数据 ----------
struct TimeShareData
{
    QString code;
    QString name;
    QString date;                 // yyyy-MM-dd
    double  prevClose = 0;        // 昨收（图表的基准线）
    QVector<MinutePoint> points;  // 按时间升序，仅含实际交易时段
    bool    valid = false;

    // 分时图横轴总格数：上午 120 + 下午 120 = 240
    static const int TOTAL_SLOTS = 240;

    // 把 "HH:mm" 映射到 0..239 的槽位；非交易时段返回 -1
    static int slotOf(const QString &time);

    // 该槽位对应的时间文字（用于横轴刻度）
    static QString slotLabel(int slot);

    // 取最后一个点
    const MinutePoint &last() const;
    bool empty() const { return points.isEmpty(); }
};

namespace TimeShare {
    // 解析东方财富 trends2 接口返回的 klines 数组
    // 每条形如: "2026-10-28 09:30,16.11,16.11,16.11,16.11,1742,2806362.00,16.110"
    // 字段: 时间,开,收,高,低,量,额,均价
    TimeShareData parse(const QString &code, const QString &name,
                        double prevClose, const QVector<QString> &lines);

    // 解析腾讯分时接口 (web.ifzq.gtimg.cn) 的 data 数组
    // 每条形如: "0930 16.11 1742 2806362.00"
    // 字段: 时间(HHmm), 价格, **累计**成交量, **累计**成交额
    // 注意：腾讯给的是全天累计值，需自行做差得到每分钟量，
    //       并用 累计额/累计量 得到均价。
    TimeShareData parseTencent(const QString &code, const QString &name,
                               double prevClose, const QString &date,
                               const QVector<QString> &lines);

    // 涨跌幅(%)，相对昨收
    double chgPct(double price, double prevClose);
}

#endif // TIMESHAREDATA_H
