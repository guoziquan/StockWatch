#include "timesharechart.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFontMetrics>
#include <cmath>

// ============= 颜色（与 KLineChart 保持一致）=============
const QColor TimeShareChart::C_UP    = QColor(0xff, 0x4d, 0x5e);
const QColor TimeShareChart::C_DOWN  = QColor(0x2e, 0xe6, 0xa8);
const QColor TimeShareChart::C_FLAT  = QColor(0x8f, 0xa3, 0xc0);
const QColor TimeShareChart::C_BG    = QColor(0x0d, 0x14, 0x22);
const QColor TimeShareChart::C_GRID  = QColor(0x16, 0x22, 0x38);
const QColor TimeShareChart::C_TEXT  = QColor(0x8f, 0xa3, 0xc0);
const QColor TimeShareChart::C_CROSS = QColor(0x4d, 0x6b, 0x99);
const QColor TimeShareChart::C_AVG   = QColor(0xff, 0xc1, 0x07);   // 均价线
const QColor TimeShareChart::C_PRICE = QColor(0xe8, 0xee, 0xfc);   // 价格线

const int TimeShareChart::L_MARGIN      = 0;
const int TimeShareChart::AXIS_W        = 58;
const int TimeShareChart::TIME_H        = 24;
const int TimeShareChart::VOL_RATIO_NUM = 22;
const int TimeShareChart::VOL_RATIO_DEN = 100;

// ============= 字体 =============
static QFont axisFont()   { return QFont(QStringLiteral("Consolas"), 8); }
static QFont timeFont()   { return QFont(QStringLiteral("Consolas"), 7); }
static QFont legendFont() { return QFont(QStringLiteral("Microsoft YaHei"), 7); }

// ============================================================================
TimeShareChart::TimeShareChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(400, 300);
    setStyleSheet(QStringLiteral("background:%1;").arg(C_BG.name()));
}

void TimeShareChart::setData(const TimeShareData &d)
{
    m_data = d;
    m_slots.clear();
    m_hover = false;
    m_hoverSlot = -1;

    // 预算每个点对应槽位
    m_slots.reserve(d.points.size());
    for (const auto &p : d.points)
        m_slots.append(TimeShareData::slotOf(p.time));

    // 涨跌停规则（用于主图上下边界与虚线）
    m_limit = PriceLimitCalc::compute(d.code, d.name, d.prevClose);

    // 价格范围：以昨收为中心对称，并把边界对齐到"整数百分比"，
    // 使轴刻度恰好落在绘图区上下边缘，既不留白也不裁切。
    double maxDev = 0;
    for (const auto &p : d.points) {
        maxDev = qMax(maxDev, qAbs(p.price - d.prevClose));
        if (p.avgPrice > 0) maxDev = qMax(maxDev, qAbs(p.avgPrice - d.prevClose));
    }
    if (d.prevClose > 0) {
        const double realPct = maxDev / d.prevClose * 100.0;   // 实际最大偏离百分比

        // 选步长：1% → 2% → 5% → 10%
        double stepPct = 1.0;
        if (realPct > 3.0)  stepPct = 2.0;
        if (realPct > 6.0)  stepPct = 5.0;
        if (realPct > 15.0) stepPct = 10.0;

        // 向上取整到步长整数倍，再加一档余量保证最高点不贴边
        int levels = int(realPct / stepPct) + 1;
        double rangePct = levels * stepPct;
        if (rangePct < 1.0) rangePct = 1.0;   // 极小波动时至少 ±1%

        // 不超过涨跌停幅度
        if (m_limit.valid) {
            const double limitPct = m_limit.limitPct;
            while (rangePct > limitPct && rangePct - stepPct >= 0.5)
                rangePct -= stepPct;
            if (rangePct > limitPct) rangePct = limitPct;
        }
        maxDev = d.prevClose * rangePct / 100.0;
    } else {
        maxDev = 1.0;
    }
    m_minP = d.prevClose - maxDev;
    m_maxP = d.prevClose + maxDev;

    update();
}

void TimeShareChart::clear()
{
    m_data = TimeShareData();
    m_slots.clear();
    m_hover = false;
    update();
}

// ============================================================================
void TimeShareChart::calcLayout()
{
    const int axisW = AXIS_W;
    const int w = width();
    const int h = height();

    const int chartTop = 4;
    const int chartBottom = h - TIME_H;
    const int totalH = qMax(120, chartBottom - chartTop);

    // 主图 : 副图 = 3 : 1。副图给足高度，成交量柱才看得出放量/缩量；
    // 主图仍占 3/4，价格走势是主角。
    const int volH = qMax(70, totalH / 4);
    const int priceH = qMax(80, totalH - volH - 4);

    // 主图价格区（左右各留出坐标轴）
    m_plotRect  = QRect(axisW, chartTop, qMax(10, w - axisW * 2), qMax(10, priceH));
    m_volRect   = QRect(axisW, m_plotRect.bottom() + 4,
                        m_plotRect.width(), volH);
    m_leftAxis  = QRect(0, chartTop, axisW, priceH);
    m_rightAxis = QRect(w - axisW, chartTop, axisW, priceH);
    m_timeAxis  = QRect(axisW, chartBottom, m_plotRect.width(), TIME_H);
}

double TimeShareChart::slotToX(double slot) const
{
    // 240 格映射到绘图区宽度；slot 0 在左边界，239 在右边界
    const double denom = TimeShareData::TOTAL_SLOTS - 1;
    if (denom <= 0) return m_plotRect.left();
    return m_plotRect.left() + slot / denom * m_plotRect.width();
}

double TimeShareChart::priceToY(double price) const
{
    if (m_maxP <= m_minP) return m_plotRect.center().y();
    const double frac = (price - m_minP) / (m_maxP - m_minP);
    return m_plotRect.bottom() - frac * m_plotRect.height();
}

// ============================================================================
void TimeShareChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), C_BG);

    calcLayout();

    if (!m_data.valid) {
        p.setPen(C_TEXT);
        p.setFont(legendFont());
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无分时数据"));
        return;
    }

    drawBackground(p);
    drawGrid(p);
    drawPriceArea(p);
    drawVolumeArea(p);
    drawAxes(p);
    drawTimeAxis(p);
    if (m_hover) drawCrosshair(p);
}

// ---------- 背景 ----------
void TimeShareChart::drawBackground(QPainter &p)
{
    // 主图区域用略深的背景区分
    p.fillRect(m_plotRect, QColor(0x0d, 0x14, 0x22));
    p.fillRect(m_volRect,  QColor(0x0b, 0x11, 0x1d));
}

// ---------- 网格与基准线 ----------
void TimeShareChart::drawGrid(QPainter &p)
{
    p.save();
    p.setClipRect(m_plotRect);

    // 横向网格（上下对称的 % 线）
    // 昨收线是重点，用亮一点的虚线
    const double midY = priceToY(m_data.prevClose);
    p.setPen(QPen(QColor(0x4a, 0x5a, 0x78), 1, Qt::DashLine));
    p.drawLine(m_plotRect.left(), int(midY), m_plotRect.right(), int(midY));

    // 涨跌停参考线（若有）
    if (m_limit.valid) {
        p.setPen(QPen(QColor(0x5a, 0x2a, 0x33), 1, Qt::DotLine));
        const double upY = priceToY(m_limit.upPrice);
        if (upY >= m_plotRect.top() && upY <= m_plotRect.bottom())
            p.drawLine(m_plotRect.left(), int(upY), m_plotRect.right(), int(upY));
        p.setPen(QPen(QColor(0x1e, 0x4a, 0x3a), 1, Qt::DotLine));
        const double dnY = priceToY(m_limit.downPrice);
        if (dnY >= m_plotRect.top() && dnY <= m_plotRect.bottom())
            p.drawLine(m_plotRect.left(), int(dnY), m_plotRect.right(), int(dnY));
    }

    p.restore();

    // 纵向网格：整点时间
    p.save();
    p.setClipRect(m_plotRect);
    p.setPen(QPen(C_GRID, 1, Qt::DotLine));
    const int ticks[] = {0, 30, 60, 90, 120, 150, 180, 210, 239};
    for (int s : ticks) {
        const int x = int(slotToX(s));
        p.drawLine(x, m_plotRect.top(), x, m_volRect.bottom());
    }
    p.restore();
}

// ---------- 价格线与填充 ----------
void TimeShareChart::drawPriceArea(QPainter &p)
{
    p.save();
    p.setClipRect(m_plotRect);

    // 先画填充（价格线与昨收线之间的区域）
    QPolygonF area;
    const double baseY = priceToY(m_data.prevClose);

    QPolygonF priceLine, avgLine;
    for (int i = 0; i < m_data.points.size(); ++i) {
        if (m_slots[i] < 0) continue;
        const double x = slotToX(m_slots[i]);
        const double y = priceToY(m_data.points[i].price);
        priceLine << QPointF(x, y);
        if (m_data.points[i].avgPrice > 0)
            avgLine << QPointF(x, priceToY(m_data.points[i].avgPrice));
    }

    if (priceLine.size() >= 2) {
        area = priceLine;
        area << QPointF(priceLine.last().x(), baseY)
             << QPointF(priceLine.first().x(), baseY);

        // 填充色取最后价格相对昨收的涨跌
        const double lastPrice = m_data.last().price;
        const bool up = lastPrice >= m_data.prevClose;
        QColor fill = up ? C_UP : C_DOWN;
        fill.setAlpha(38);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPolygon(area);

        // 价格线
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(C_PRICE, 1.2));
        p.drawPolyline(priceLine);

        // 均价线
        if (avgLine.size() >= 2) {
            p.setPen(QPen(C_AVG, 1.0));
            p.drawPolyline(avgLine);
        }

        // 最新价格点高亮圆点
        p.setPen(Qt::NoPen);
        p.setBrush(lastPrice >= m_data.prevClose ? C_UP : C_DOWN);
        p.drawEllipse(priceLine.last(), 2.6, 2.6);
    }

    p.restore();
}

// ---------- 成交量 ----------
void TimeShareChart::drawVolumeArea(QPainter &p)
{
    p.save();
    p.setClipRect(m_volRect);

    double maxVol = 0;
    for (const auto &pt : m_data.points) maxVol = qMax(maxVol, pt.volume);
    if (maxVol <= 0) maxVol = 1;

    // 每分钟柱宽：按 240 格计算
    const double step = double(m_volRect.width()) / (TimeShareData::TOTAL_SLOTS - 1);
    const double barW = qMax(0.8, step * 0.7);

    for (int i = 0; i < m_data.points.size(); ++i) {
        if (m_slots[i] < 0) continue;
        const auto &pt = m_data.points[i];
        // 涨跌色取该分钟相对前一分钟的变化
        const double prevP = (i > 0) ? m_data.points[i - 1].price : m_data.prevClose;
        const bool up = pt.price >= prevP;
        QColor c = up ? C_UP : C_DOWN;
        c.setAlpha(150);

        const double h = pt.volume / maxVol * m_volRect.height();
        const double x = slotToX(m_slots[i]) - barW / 2;
        p.fillRect(QRectF(x, m_volRect.bottom() - h, barW, qMax(1.0, h)), c);
    }
    p.restore();
}

// ---------- 左右坐标轴 ----------
void TimeShareChart::drawAxes(QPainter &p)
{
    p.save();
    p.setFont(axisFont());
    QFontMetrics fm(p.font());

    // 左/右轴：以"整数百分比"为刻度（A股分时图惯例）。
    // 价格范围已在 setData 中对齐到整数百分比，因此上下边缘本身就是刻度。
    if (m_data.prevClose <= 0) { p.restore(); return; }

    // 反推步长：范围是 ±rangePct，用 1/2/5 档划分，保证上下对称
    const double rangePct = (m_maxP - m_data.prevClose) / m_data.prevClose * 100.0;
    double stepPct = 1.0;
    if (rangePct > 3.0)  stepPct = 2.0;
    if (rangePct > 6.0)  stepPct = 5.0;
    if (rangePct > 15.0) stepPct = 10.0;

    const int ticks = qMax(1, int(rangePct / stepPct + 0.5));
    const double realStep = rangePct / ticks;   // 实际步长（可能非整数，但保证对称）

    for (int i = -ticks; i <= ticks; ++i) {
        const double pct = i * realStep;
        const double price = m_data.prevClose * (1.0 + pct / 100.0);

        const int y = int(priceToY(price));
        int ty = y - fm.height() / 2;
        ty = qBound(m_plotRect.top(), ty, m_plotRect.bottom() - fm.height());

        // 左轴：价格
        const QString txt = QString::number(price, 'f', 2);
        const int tw = fm.width(txt);
        p.setPen(C_TEXT);
        p.drawText(QRect(m_plotRect.left() - tw - 6, ty, tw, fm.height()),
                   Qt::AlignRight | Qt::AlignVCenter, txt);

        // 右轴：涨跌幅（整数百分比优先，否则一位小数）
        QString ps;
        if (qAbs(pct - qRound(pct)) < 0.01)
            ps = QStringLiteral("%1%2%").arg(pct > 0 ? QStringLiteral("+") : QString())
                                         .arg(pct, 0, 'f', 0);
        else
            ps = QStringLiteral("%1%2%").arg(pct > 0 ? QStringLiteral("+") : QString())
                                         .arg(pct, 0, 'f', 1);
        QColor c = (pct > 0.0001) ? C_UP : ((pct < -0.0001) ? C_DOWN : C_FLAT);
        p.setPen(c);
        const int pw = fm.width(ps);
        p.drawText(QRect(m_plotRect.right() + 6, ty, pw, fm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, ps);
    }

    // 成交量轴：最大值标注
    double maxVol = 0;
    for (const auto &pt : m_data.points) maxVol = qMax(maxVol, pt.volume);
    if (maxVol > 0) {
        QString v = (maxVol >= 1e4)
                        ? QString::number(maxVol / 1e4, 'f', 0) + QStringLiteral("万手")
                        : QString::number(maxVol, 'f', 0) + QStringLiteral("手");
        p.setPen(C_TEXT);
        int vw = fm.width(v);
        p.drawText(QRect(m_plotRect.left() - vw - 6,
                         m_volRect.top() - 2, vw, fm.height()),
                   Qt::AlignRight | Qt::AlignTop, v);
    }

    // 图例：价格线 / 均价线
    p.setFont(legendFont());
    int lx = m_plotRect.left() + 4;
    const int ly = m_plotRect.top() + 2;
    auto legend = [&](const QString &txt, const QColor &c) {
        p.setPen(c);
        const int tw = p.fontMetrics().width(txt) + 8;
        p.drawText(lx, ly, tw, 13, Qt::AlignLeft, txt);
        lx += tw;
    };
    legend(QStringLiteral("价格"), C_PRICE);
    legend(QStringLiteral("均价"), C_AVG);
    if (m_limit.valid) {
        p.setPen(C_TEXT);
        p.drawText(lx, ly, 160, 13, Qt::AlignLeft,
                   QStringLiteral("| %1 ±%2%")
                       .arg(PriceLimitCalc::boardName(m_limit.board))
                       .arg(m_limit.limitPct, 0, 'f', 0));
    }

    p.restore();
}

// ---------- 时间轴 ----------
void TimeShareChart::drawTimeAxis(QPainter &p)
{
    p.save();
    p.setFont(timeFont());
    p.setPen(C_TEXT);
    QFontMetrics fm(p.font());

    // 标注 09:30 / 10:30 / 11:30-13:00 / 14:00 / 15:00
    struct { int slot; const char *label; } marks[] = {
        {   0, "09:30" },
        {  60, "10:30" },
        { 120, "11:30/13:00" },
        { 180, "14:00" },
        { 239, "15:00" }
    };

    for (const auto &m : marks) {
        const QString label = QString::fromUtf8(m.label);
        const int tw = fm.width(label);
        int x = int(slotToX(m.slot)) - tw / 2;
        x = qBound(m_timeAxis.left(), x, m_timeAxis.right() - tw);
        p.drawText(QRect(x, m_timeAxis.top(), tw, m_timeAxis.height()),
                   Qt::AlignCenter, label);
    }
    p.restore();
}

// ---------- 十字光标 ----------
void TimeShareChart::drawCrosshair(QPainter &p)
{
    p.save();
    p.setFont(axisFont());
    QFontMetrics fm(p.font());

    // 找最近的分钟点
    int bestIdx = -1;
    double bestDist = 1e9;
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i] < 0) continue;
        const double x = slotToX(m_slots[i]);
        const double dist = qAbs(x - m_mousePos.x());
        if (dist < bestDist) { bestDist = dist; bestIdx = i; }
    }
    if (bestIdx < 0) { p.restore(); return; }

    const double x = slotToX(m_slots[bestIdx]);
    const int my = m_mousePos.y();

    // 竖线（贯穿主图与副图）
    p.setPen(QPen(C_CROSS, 1, Qt::DashLine));
    p.drawLine(int(x), m_plotRect.top(), int(x), m_volRect.bottom());

    // 横线（仅主图/副图之一）
    if (m_plotRect.contains(m_mousePos))
        p.drawLine(m_plotRect.left(), my, m_plotRect.right(), my);
    else if (m_volRect.contains(m_mousePos))
        p.drawLine(m_volRect.left(), my, m_volRect.right(), my);

    // ---- 浮标 ----
    auto drawTag = [&](const QRect &r, const QString &text, const QColor &fg,
                       const QColor &border) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x2a, 0x3d, 0x5c, 235));
        p.drawRoundedRect(r, 2, 2);
        p.setPen(QPen(border, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 2, 2);
        p.setPen(fg);
        p.drawText(r, Qt::AlignCenter, text);
    };

    const int th = fm.height() + 4;

    // 价格浮标（左轴）
    if (m_plotRect.contains(m_mousePos)) {
        const double frac = double(m_plotRect.bottom() - my) / m_plotRect.height();
        const double price = m_minP + frac * (m_maxP - m_minP);
        const QString t = QString::number(price, 'f', 2);
        const int tw = fm.width(t) + 10;
        int ty = qBound(m_plotRect.top(), my - th / 2, m_plotRect.bottom() - th);
        drawTag(QRect(AXIS_W + 2, ty, tw, th), t,
                QColor(0xff, 0xe0, 0x8a), QColor(0x4d, 0xa3, 0xff));
    }

    // 时间浮标
    {
        const QString t = m_data.points[bestIdx].time;
        const int tw = fm.width(t) + 10;
        int tx = qBound(m_timeAxis.left(), int(x) - tw / 2, m_timeAxis.right() - tw);
        drawTag(QRect(tx, m_timeAxis.top() + 2, tw, th), t,
                QColor(0xe8, 0xee, 0xfc), QColor(0x4d, 0xa3, 0xff));
    }

    // 数据浮标：显示该分钟的价格 / 均价 / 成交量
    {
        const auto &pt = m_data.points[bestIdx];
        const QString t1 = QStringLiteral("价 %1").arg(pt.price, 0, 'f', 2);
        QString t2 = QStringLiteral("均 %1").arg(pt.avgPrice, 0, 'f', 2);
        QString t3;
        if (pt.volume >= 1e4)
            t3 = QStringLiteral("量 %1万").arg(pt.volume / 1e4, 0, 'f', 1);
        else
            t3 = QStringLiteral("量 %1").arg(pt.volume, 0, 'f', 0);
        const QString full = t1 + QStringLiteral("  ") + t2 + QStringLiteral("  ") + t3;

        p.setFont(legendFont());
        const int tw = p.fontMetrics().width(full) + 12;
        int tx = qBound(m_plotRect.left(), int(x) + 8, m_plotRect.right() - tw);
        const int ty2 = m_plotRect.top() + 18;
        const QColor c = (pt.price >= m_data.prevClose) ? C_UP : C_DOWN;
        drawTag(QRect(tx, ty2, tw, th), full, c, QColor(0x4d, 0xa3, 0xff));
    }

    p.restore();
}

// ============================================================================
void TimeShareChart::mouseMoveEvent(QMouseEvent *ev)
{
    m_mousePos = ev->pos();
    if (m_plotRect.contains(ev->pos()) || m_volRect.contains(ev->pos())) {
        m_hover = true;
        update();
    } else if (m_hover) {
        m_hover = false;
        update();
    }
}

void TimeShareChart::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}
