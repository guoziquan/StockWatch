#ifndef SECTORDATA_H
#define SECTORDATA_H

#include <QString>
#include <QStringList>
#include <QVector>

// ============================================================================
// 板块联动与相对强度
//
// 为什么要做这个：
//   A 股个股涨跌约 60% 由板块决定。同样的 +3%，可能是板块 +5% 里的"跟风"，
//   也可能是板块 +0.5% 里的"独立走强" —— 含义完全不同：
//     * 领涨（个股显著强于板块）：有独立逻辑或资金，持续性通常更好
//     * 跟风（个股弱于板块）：涨是因为板块在涨，板块一回落就跌得更快
//     * 拖累（个股显著弱于板块）：板块涨它不涨，往往有个股自身问题
//
// 本模块提供纯计算部分（可测试），网络请求在 MainWindow 中完成。
// ============================================================================

// ---------- 板块类型 ----------
enum class SectorKind {
    Industry,   // 行业板块（如 传媒、游戏Ⅱ）
    Concept,    // 概念板块（如 网络游戏、元宇宙概念）
    Region,     // 地域板块（如 安徽板块）
    Unknown
};

// ---------- 单个板块 ----------
struct SectorItem
{
    QString     code;         // BK0486
    QString     name;         // 传媒
    SectorKind  kind = SectorKind::Unknown;
    double      changePct = 0;// 板块涨跌幅(%)
    bool        valid() const { return !code.isEmpty() && !name.isEmpty(); }
};

// ---------- 相对强度评价 ----------
enum class StrengthLevel {
    NoData,        // 数据不足
    Leader,        // 领涨（显著强于板块）
    SlightlyUp,    // 略强于板块
    InLine,        // 与板块同步
    SlightlyDown,  // 略弱于板块
    Laggard        // 拖累（显著弱于板块）
};

// ---------- 个股 vs 板块 的联动结果 ----------
struct SectorLink
{
    QString  stockCode;
    QString  stockName;
    double   stockPct = 0;        // 个股涨跌幅(%)
    QVector<SectorItem> sectors;  // 所属板块（按涨幅降序）

    // 主板块（用于判断联动）：优先行业，其次概念
    SectorItem primary;
    bool       hasPrimary = false;

    // 相对强度 = 个股涨幅 - 主板块涨幅
    double        relStrength = 0;
    StrengthLevel level = StrengthLevel::NoData;
    QString       verdict;        // 一句话结论

    bool valid() const { return hasPrimary; }
};

namespace SectorCalc {

// 根据板块代码前缀/名称推断板块类型
SectorKind kindOf(const QString &code, const QString &name);

// 从个股所属板块列表中挑出"主板块"：
//   优先行业板块（最能代表该股的赛道），
//   没有行业板块时退回概念板块中涨幅最具代表性的一个。
//   地域板块不参与主板块选择（它不决定个股走势）。
SectorItem pickPrimary(const QVector<SectorItem> &sectors);

// 判定相对强度等级
//   diff 为 个股涨幅 - 板块涨幅（百分点）
StrengthLevel levelOf(double diff);

// 生成一句话结论
QString verdictOf(double stockPct, double sectorPct, StrengthLevel level);

// 核心：组装联动结果
SectorLink build(const QString &code, const QString &name,
                 double stockPct, const QVector<SectorItem> &sectors);

// 把板块列表按涨幅降序排序
QVector<SectorItem> sortedByPct(const QVector<SectorItem> &sectors);

// 等级中文名
QString levelName(StrengthLevel l);
// 板块类型中文名
QString kindName(SectorKind k);

} // namespace SectorCalc

#endif // SECTORDATA_H
