#include "sectordata.h"

#include <algorithm>
#include <cmath>

namespace SectorCalc {

SectorKind kindOf(const QString &code, const QString &name)
{
    Q_UNUSED(code);

    // 地域板块：东财地域板块统一以"板块"结尾（如 安徽板块、广东板块）
    if (name.endsWith(QStringLiteral("板块")) && name.size() <= 8)
        return SectorKind::Region;

    // 行业板块特征一：名称带罗马数字后缀（游戏Ⅱ、游戏Ⅲ、银行Ⅱ 等）
    if (name.endsWith(QChar(0x2161))      // Ⅱ
        || name.endsWith(QChar(0x2162))   // Ⅲ
        || name.endsWith(QChar(0x2160)))  // Ⅰ
        return SectorKind::Industry;

    // 行业板块特征二：东财部分行业板块名称不带罗马数字（如 传媒、电力、证券）。
    // 这些名称数量有限且稳定，用白名单识别比猜测可靠。
    static const QStringList industries = {
        QStringLiteral("传媒"),   QStringLiteral("电力"),   QStringLiteral("证券"),
        QStringLiteral("银行"),   QStringLiteral("保险"),   QStringLiteral("地产"),
        QStringLiteral("房地产"), QStringLiteral("煤炭"),   QStringLiteral("钢铁"),
        QStringLiteral("有色"),   QStringLiteral("化工"),   QStringLiteral("建材"),
        QStringLiteral("机械"),   QStringLiteral("汽车"),   QStringLiteral("家电"),
        QStringLiteral("食品"),   QStringLiteral("饮料"),   QStringLiteral("白酒"),
        QStringLiteral("医药"),   QStringLiteral("医疗"),   QStringLiteral("农业"),
        QStringLiteral("纺织"),   QStringLiteral("服装"),   QStringLiteral("商贸"),
        QStringLiteral("零售"),   QStringLiteral("交运"),   QStringLiteral("运输"),
        QStringLiteral("建筑"),   QStringLiteral("装饰"),   QStringLiteral("环保"),
        QStringLiteral("军工"),   QStringLiteral("国防"),   QStringLiteral("计算机"),
        QStringLiteral("通信"),   QStringLiteral("电子"),   QStringLiteral("半导体"),
        QStringLiteral("软件"),   QStringLiteral("互联网"), QStringLiteral("教育"),
        QStringLiteral("旅游"),   QStringLiteral("酒店"),   QStringLiteral("餐饮"),
        QStringLiteral("航空"),   QStringLiteral("港口"),   QStringLiteral("公路")
    };
    for (const QString &ind : industries) {
        if (name == ind) return SectorKind::Industry;
    }

    // 其余按概念处理（网络游戏、元宇宙概念、华为概念…）
    return SectorKind::Concept;
}

SectorItem pickPrimary(const QVector<SectorItem> &sectors)
{
    // 1) 优先行业板块。若有多个行业板块（如同时属"传媒"和"游戏Ⅱ"），
    //    取最细分的那个 —— 名称更长/带更高罗马数字的通常更具体。
    SectorItem best;
    bool hasIndustry = false;
    for (const auto &s : sectors) {
        if (s.kind != SectorKind::Industry) continue;
        if (!s.valid()) continue;
        if (!hasIndustry) { best = s; hasIndustry = true; continue; }
        // 更细分的行业优先（名称带 Ⅱ/Ⅲ 的比不带后缀的更具体）
        const bool curSpecific = best.name.endsWith(QChar(0x2161))
                              || best.name.endsWith(QChar(0x2162));
        const bool newSpecific = s.name.endsWith(QChar(0x2161))
                              || s.name.endsWith(QChar(0x2162));
        if (newSpecific && !curSpecific) best = s;
    }
    if (hasIndustry) return best;

    // 2) 没有行业板块时，从概念板块里挑一个代表。
    //    优先"概念"结尾的（更明确是主题概念），否则取第一个有效概念。
    for (const auto &s : sectors) {
        if (s.kind == SectorKind::Concept && s.valid()
            && s.name.endsWith(QStringLiteral("概念")))
            return s;
    }
    for (const auto &s : sectors) {
        if (s.kind == SectorKind::Concept && s.valid()) return s;
    }

    // 3) 兜底：任意有效板块（但不选地域）
    for (const auto &s : sectors) {
        if (s.kind != SectorKind::Region && s.valid()) return s;
    }
    return SectorItem();
}

StrengthLevel levelOf(double diff)
{
    // 阈值说明（单位：百分点）：
    //   ≥ +3   领涨：明显走出独立行情
    //   +1~+3  略强
    //   -1~+1  同步
    //   -3~-1  略弱
    //   ≤ -3   拖累
    if (diff >= 3.0)  return StrengthLevel::Leader;
    if (diff >= 1.0)  return StrengthLevel::SlightlyUp;
    if (diff > -1.0)  return StrengthLevel::InLine;
    if (diff > -3.0)  return StrengthLevel::SlightlyDown;
    return StrengthLevel::Laggard;
}

QString levelName(StrengthLevel l)
{
    switch (l) {
    case StrengthLevel::Leader:       return QStringLiteral("领涨");
    case StrengthLevel::SlightlyUp:   return QStringLiteral("略强");
    case StrengthLevel::InLine:       return QStringLiteral("同步");
    case StrengthLevel::SlightlyDown: return QStringLiteral("略弱");
    case StrengthLevel::Laggard:      return QStringLiteral("拖累");
    case StrengthLevel::NoData:       break;
    }
    return QStringLiteral("无数据");
}

QString kindName(SectorKind k)
{
    switch (k) {
    case SectorKind::Industry: return QStringLiteral("行业");
    case SectorKind::Concept:  return QStringLiteral("概念");
    case SectorKind::Region:   return QStringLiteral("地域");
    case SectorKind::Unknown:  break;
    }
    return QStringLiteral("其他");
}

QString verdictOf(double stockPct, double sectorPct, StrengthLevel level)
{
    const double diff = stockPct - sectorPct;

    switch (level) {
    case StrengthLevel::Leader:
        return QStringLiteral("板块%1%而个股%2%，领先板块 %3 个百分点，"
                              "走出独立强势行情")
            .arg(sectorPct, 0, 'f', 2).arg(stockPct, 0, 'f', 2)
            .arg(diff, 0, 'f', 2);
    case StrengthLevel::SlightlyUp:
        return QStringLiteral("略强于板块（%1 个百分点），有一定独立性")
            .arg(diff, 0, 'f', 2);
    case StrengthLevel::InLine:
        return QStringLiteral("与板块基本同步（差异 %1 个百分点），"
                              "走势主要由板块驱动")
            .arg(diff, 0, 'f', 2);
    case StrengthLevel::SlightlyDown:
        return QStringLiteral("弱于板块（%1 个百分点），上涨主要靠板块带动")
            .arg(-diff, 0, 'f', 2);
    case StrengthLevel::Laggard:
        return QStringLiteral("明显弱于板块（落后 %1 个百分点），"
                              "板块走强但个股不涨，需警惕个股自身问题")
            .arg(-diff, 0, 'f', 2);
    case StrengthLevel::NoData:
        break;
    }
    return QStringLiteral("数据不足，无法判断联动");
}

SectorLink build(const QString &code, const QString &name,
                 double stockPct, const QVector<SectorItem> &sectors)
{
    SectorLink link;
    link.stockCode = code;
    link.stockName = name;
    link.stockPct  = stockPct;
    link.sectors   = sortedByPct(sectors);

    const SectorItem primary = pickPrimary(sectors);
    if (primary.valid()) {
        link.primary    = primary;
        link.hasPrimary = true;
        link.relStrength = stockPct - primary.changePct;
        link.level = levelOf(link.relStrength);
        link.verdict = verdictOf(stockPct, primary.changePct, link.level);
    } else {
        link.hasPrimary = false;
        link.level = StrengthLevel::NoData;
        link.verdict = QStringLiteral("未获取到所属板块数据");
    }
    return link;
}

QVector<SectorItem> sortedByPct(const QVector<SectorItem> &sectors)
{
    QVector<SectorItem> out = sectors;
    std::stable_sort(out.begin(), out.end(),
                     [](const SectorItem &a, const SectorItem &b) {
        return a.changePct > b.changePct;
    });
    return out;
}

} // namespace SectorCalc
