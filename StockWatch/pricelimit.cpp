#include "pricelimit.h"

#include <cmath>
#include <cstdint>

namespace {

// 把 double 价格安全转换为"分"（四舍五入到最近的整数分）
int64_t toCents(double price)
{
    // 先放大 100 倍并按"靠近整分"处理，避免 14.499999 被截成 1449
    const double scaled = price * 100.0;
    return static_cast<int64_t>(std::floor(scaled + 0.5));
}

// 以"分"为单位计算涨跌停价，返回"分"
//   sign = +1 涨停, -1 跌停
int64_t limitCents(int64_t prevCloseCents, int limitPct, int sign)
{
    // 目标 = prevCloseCents * (100 ± pct) / 100
    const int64_t numerator = prevCloseCents * (100 + sign * limitPct);
    const int64_t denom = 100;

    // 四舍五入：先加半个分母再整除
    //   numerator 恒为正（涨跌停价都 > 0），可直接用正数舍入
    const int64_t q = (numerator + denom / 2) / denom;
    return q;
}

double fromCents(int64_t cents)
{
    return static_cast<double>(cents) / 100.0;
}

} // namespace

namespace PriceLimitCalc {

BoardType boardOf(const QString &code)
{
    if (code.size() != 6) return BoardType::Unknown;

    // 取前三位判断，注意优先级：三位前缀比两位更精确
    const QString p3 = code.left(3);
    const QString p2 = code.left(2);

    // 科创板
    if (p3 == QLatin1String("688") || p3 == QLatin1String("689"))
        return BoardType::StarMarket;

    // 创业板
    if (p3 == QLatin1String("300") || p3 == QLatin1String("301"))
        return BoardType::ChiNext;

    // 沪市主板
    if (p3 == QLatin1String("600") || p3 == QLatin1String("601")
        || p3 == QLatin1String("603") || p3 == QLatin1String("605"))
        return BoardType::MainSh;

    // 深市主板
    if (p3 == QLatin1String("000") || p3 == QLatin1String("001"))
        return BoardType::MainSz;

    // 北交所：43xxxx / 83xxxx / 87xxxx / 920xxx
    if (p2 == QLatin1String("43") || p2 == QLatin1String("83")
        || p2 == QLatin1String("87") || p3 == QLatin1String("920"))
        return BoardType::BSE;

    // 兜底：6 开头视为沪市，其余视为深市
    if (code.startsWith(QLatin1Char('6'))) return BoardType::MainSh;
    return BoardType::MainSz;
}

RiskStatus riskStatusOf(const QString &name)
{
    // 去掉可能的空格再判断
    QString n = name;
    n.remove(QLatin1Char(' '));

    // *ST 优先（更严重）
    if (n.startsWith(QLatin1String("*ST")) || n.startsWith(QLatin1String("＊ST")))
        return RiskStatus::StarST;
    if (n.startsWith(QLatin1String("ST")) || n.startsWith(QLatin1String("ＳＴ")))
        return RiskStatus::ST;

    // 名称中间含 ST（如 "ST佳缘" 之外的变体）
    if (n.contains(QLatin1String("ST")))
        return RiskStatus::ST;

    return RiskStatus::Normal;
}

double limitPctOf(BoardType board, RiskStatus risk)
{
    switch (board) {
    case BoardType::ChiNext:
    case BoardType::StarMarket:
        // 重要：创业板/科创板的 ST 股仍是 ±20%，不是 ±5%
        return 20.0;
    case BoardType::BSE:
        return 30.0;
    case BoardType::MainSh:
    case BoardType::MainSz:
        // 只有主板的 ST 股才缩窄到 ±5%
        return (risk == RiskStatus::Normal) ? 10.0 : 5.0;
    case BoardType::Unknown:
        break;
    }
    return 10.0;
}

PriceLimit compute(const QString &code, const QString &name, double prevClose)
{
    PriceLimit pl;
    pl.board = boardOf(code);
    pl.risk  = riskStatusOf(name);
    pl.limitPct = limitPctOf(pl.board, pl.risk);
    pl.explanation = QStringLiteral("%1 · %2 · 涨跌幅限制 ±%3%")
                         .arg(boardName(pl.board), riskName(pl.risk))
                         .arg(pl.limitPct, 0, 'f', 0);

    if (prevClose <= 0.0) {
        pl.valid = false;
        return pl;
    }

    // 以"分"为单位的整数运算，规避浮点误差（详见文件头的说明）
    const int64_t pcCents = toCents(prevClose);
    const int pct = static_cast<int>(pl.limitPct);
    pl.upPrice   = fromCents(limitCents(pcCents, pct, +1));
    pl.downPrice = fromCents(limitCents(pcCents, pct, -1));
    pl.valid = true;

    return pl;
}

bool isLimitUp(double price, const PriceLimit &pl)
{
    if (!pl.valid || pl.noLimit || price <= 0) return false;
    return price >= pl.upPrice - 0.005;
}

bool isLimitDown(double price, const PriceLimit &pl)
{
    if (!pl.valid || pl.noLimit || price <= 0) return false;
    return price <= pl.downPrice + 0.005;
}

double distToUpPct(double price, const PriceLimit &pl)
{
    if (!pl.valid || price <= 0 || pl.upPrice <= 0) return 0.0;
    return (pl.upPrice - price) / price * 100.0;
}

double distToDownPct(double price, const PriceLimit &pl)
{
    if (!pl.valid || price <= 0 || pl.downPrice <= 0) return 0.0;
    return (price - pl.downPrice) / price * 100.0;
}

QString boardName(BoardType b)
{
    switch (b) {
    case BoardType::MainSh:     return QStringLiteral("沪市主板");
    case BoardType::MainSz:     return QStringLiteral("深市主板");
    case BoardType::ChiNext:    return QStringLiteral("创业板");
    case BoardType::StarMarket: return QStringLiteral("科创板");
    case BoardType::BSE:        return QStringLiteral("北交所");
    case BoardType::Unknown:    break;
    }
    return QStringLiteral("未知板块");
}

QString riskName(RiskStatus r)
{
    switch (r) {
    case RiskStatus::ST:     return QStringLiteral("ST");
    case RiskStatus::StarST: return QStringLiteral("*ST");
    case RiskStatus::Normal: break;
    }
    return QStringLiteral("正常");
}

} // namespace PriceLimitCalc
