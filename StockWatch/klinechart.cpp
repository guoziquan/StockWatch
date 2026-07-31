#include "klinechart.h"

#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QtMath>

const QColor KLineChart::COLOR_UP   = QColor(0xff, 0x4d, 0x5e);
const QColor KLineChart::COLOR_DOWN = QColor(0x2e, 0xe6, 0xa8);
const QColor KLineChart::COLOR_BG   = QColor(0x0d, 0x14, 0x22);
const QColor KLineChart::COLOR_GRID = QColor(0x1a, 0x27, 0x3f);
const QColor KLineChart::COLOR_TEXT = QColor(0x8f, 0xa3, 0xc0);

KLineChart::KLineChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(200, 200);
    setStyleSheet(QString("background:%1;").arg(COLOR_BG.name()));
}

void KLineChart::setData(const QVector<KLineData> &data)
{
    m_data = data;
    m_scrollOffset = 0;
    m_crosshair = false;
    m_crossIdx = -1;
    update();
}

void KLineChart::clear()
{
    m_data.clear();
    m_scrollOffset = 0;
    update();
}

// ---------- 布局计算 ----------
int KLineChart::candleWidth() const { return m_candleW + m_gap; }

int KLineChart::visibleCount() const
{
    int w = width() - MARGIN_L - MARGIN_R;
    int cw = candleWidth();
    return cw > 0 ? qMax(1, w / cw) : width() / 8;
}

int KLineChart::idxAtPos(int x) const
{
    int vis = visibleCount();
    int start = m_data.size() - m_scrollOffset - vis;
    int cw = candleWidth();
    if (cw <= 0) return -1;
    int i = start + (x - MARGIN_L) / cw;
    if (i < 0 || i >= m_data.size()) return -1;
    return i;
}

// ---------- 绘制 ----------
void KLineChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();
    int kh = static_cast<int>((h - MARGIN_T - MARGIN_B - GAP_MID) * m_klineRatio);
    int vh = h - MARGIN_T - MARGIN_B - GAP_MID - kh;

    QRect kArea(MARGIN_L, MARGIN_T, w - MARGIN_L - MARGIN_R, kh);
    QRect vArea(MARGIN_L, MARGIN_T + kh + GAP_MID, w - MARGIN_L - MARGIN_R, vh);

    drawCandles(p, kArea);
    drawVolumes(p, vArea);
    drawAxis(p, kArea, vArea);
    drawCrosshair(p);
}

void KLineChart::drawCandles(QPainter &p, const QRect &area)
{
    if (m_data.isEmpty()) {
        p.setPen(COLOR_TEXT);
        p.drawText(area, Qt::AlignCenter, QStringLiteral("加载中…"));
        return;
    }

    int vis = visibleCount();
    int start = qMax(0, m_data.size() - m_scrollOffset - vis);
    int cw = candleWidth();

    // 计算 y 轴范围
    double maxP = 0, minP = 1e15;
    for (int i = start; i < m_data.size() && i < start + vis; ++i) {
        if (m_data[i].high > maxP) maxP = m_data[i].high;
        if (m_data[i].low  < minP) minP = m_data[i].low;
    }
    if (maxP == minP) { maxP += 0.1; minP -= 0.1; }
    double range = maxP - minP;
    double pxPerUnit = (area.height() - 2) / range;

    auto yPos = [&](double price) -> double {
        return area.bottom() - (price - minP) * pxPerUnit;
    };

    // 网格
    p.setPen(QPen(COLOR_GRID, 1));
    int sections = qMax(2, qMin(8, area.height() / 60));
    for (int i = 1; i < sections; ++i) {
        int y = area.top() + area.height() * i / sections;
        p.drawLine(area.left(), y, area.right(), y);
    }

    // 蜡烛
    double halfW = m_candleW * 0.45;
    for (int i = start, cnt = 0; i < m_data.size() && cnt < vis; ++i, ++cnt) {
        const auto &d = m_data[i];
        int cx = area.left() + cnt * cw + cw / 2;
        bool up = d.close >= d.open;
        QColor color = up ? COLOR_UP : COLOR_DOWN;

        double y1 = yPos(up ? d.close : d.open);
        double y2 = yPos(up ? d.open  : d.close);
        double yH = yPos(d.high);
        double yL = yPos(d.low);

        // 影线
        p.setPen(QPen(color, 1));
        p.drawLine(QPointF(cx, yH), QPointF(cx, yL));

        // 实体
        double bodyH = qAbs(y1 - y2);
        if (bodyH < 0.5) bodyH = 0.5;
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRect(QRectF(cx - halfW, qMin(y1, y2), m_candleW * 0.9, bodyH));
    }
}

void KLineChart::drawVolumes(QPainter &p, const QRect &area)
{
    if (m_data.isEmpty()) return;
    int vis = visibleCount();
    int start = qMax(0, m_data.size() - m_scrollOffset - vis);
    int cw = candleWidth();

    double maxV = 0;
    for (int i = start; i < m_data.size() && i < start + vis; ++i)
        if (m_data[i].volume > maxV) maxV = m_data[i].volume;
    if (maxV == 0) maxV = 1;

    double pxPerUnit = area.height() / maxV;
    double halfW = m_candleW * 0.45;

    for (int i = start, cnt = 0; i < m_data.size() && cnt < vis; ++i, ++cnt) {
        const auto &d = m_data[i];
        int cx = area.left() + cnt * cw + cw / 2;
        bool up = d.close >= d.open;
        QColor color = up ? COLOR_UP : COLOR_DOWN;
        double vh = d.volume * pxPerUnit;
        if (vh < 0.5) vh = 0.5;
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 120));
        p.setPen(Qt::NoPen);
        p.drawRect(QRectF(cx - halfW, area.bottom() - vh, m_candleW * 0.9, vh));
    }
}

void KLineChart::drawAxis(QPainter &p, const QRect &ka, const QRect &va)
{
    if (m_data.isEmpty()) return;
    int vis = visibleCount();
    int start = qMax(0, m_data.size() - m_scrollOffset - vis);
    int cw = candleWidth();

    // ---- 主图 Y 轴价格标签 ----
    double maxP = 0, minP = 1e15;
    for (int i = start; i < m_data.size() && i < start + vis; ++i) {
        if (m_data[i].high > maxP) maxP = m_data[i].high;
        if (m_data[i].low  < minP) minP = m_data[i].low;
    }
    if (maxP == minP) { maxP += 0.1; minP -= 0.1; }

    p.setPen(COLOR_TEXT);
    p.setFont(QFont("Consolas, Microsoft YaHei", 9));
    int sections = qMax(2, qMin(8, ka.height() / 60));
    for (int i = 0; i <= sections; ++i) {
        double price = maxP - (maxP - minP) * i / sections;
        int y = ka.top() + ka.height() * i / sections;
        p.drawText(QRectF(ka.right() + 4, y - 10, 70, 20),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(price, 'f', 2));
    }

    // ---- 量图 Y 轴标签 ----
    double maxV = 0;
    for (int i = start; i < m_data.size() && i < start + vis; ++i)
        if (m_data[i].volume > maxV) maxV = m_data[i].volume;
    if (maxV == 0) maxV = 1;

    auto volLabel = [&](double v) -> QString {
        if (v >= 1e8) return QString::number(v / 1e8, 'f', 1) + QStringLiteral("亿");
        if (v >= 1e4) return QString::number(v / 1e4, 'f', 0) + QStringLiteral("万");
        return QString::number(v, 'f', 0);
    };
    for (int i = 0; i <= 2; ++i) {
        double v = maxV * i / 2.0;
        int y = va.bottom() - va.height() * i / 2;
        p.drawText(QRectF(va.right() + 4, y - 10, 70, 20),
                   Qt::AlignLeft | Qt::AlignVCenter, volLabel(v));
    }

    // ---- X 轴日期标签（每隔 10 根左右） ----
    int step = qMax(1, vis / 8);
    p.setFont(QFont("Consolas, Microsoft YaHei", 8));
    for (int i = start, cnt = 0; i < m_data.size() && cnt < vis; ++i, ++cnt) {
        if (cnt % step != 0) continue;
        int cx = ka.left() + cnt * cw + cw / 2;
        QString label = m_data[i].date.right(5);  // 只显示 MM-DD
        p.drawText(QRectF(cx - 30, ka.bottom() + 2, 60, 14),
                   Qt::AlignCenter, label);
    }
}

void KLineChart::drawCrosshair(QPainter &p)
{
    if (!m_crosshair || m_crossIdx < 0 || m_crossIdx >= m_data.size())
        return;

    int vis = visibleCount();
    int start = m_data.size() - m_scrollOffset - vis;
    int cw = candleWidth();
    int cnt = m_crossIdx - start;
    int cx = MARGIN_L + cnt * cw + cw / 2;
    int w = width() - MARGIN_R;
    int kh = static_cast<int>((height() - MARGIN_T - MARGIN_B - GAP_MID) * m_klineRatio);

    p.setPen(QPen(COLOR_TEXT, 1, Qt::DashLine));
    p.drawLine(cx, MARGIN_T, cx, MARGIN_T + kh);
    p.drawLine(MARGIN_L, m_crossPos.y(), w, m_crossPos.y());

    // tooltip
    const auto &d = m_data[m_crossIdx];
    QString tip = QStringLiteral("%1\n开:%2  收:%3\n高:%4  低:%5  量:%6")
        .arg(d.date)
        .arg(d.open).arg(d.close)
        .arg(d.high).arg(d.low)
        .arg(d.volume, 0, 'f', 0);

    QToolTip::showText(mapToGlobal(QPoint(cx + 8, m_crossPos.y() - 40)), tip, this);
}

// ---------- 交互 ----------
void KLineChart::wheelEvent(QWheelEvent *ev)
{
    int delta = ev->angleDelta().y();
    if (delta > 0) {
        m_candleW = qMin(30, m_candleW + 1);
        m_gap = qMin(4, m_gap + 1);
    } else {
        m_candleW = qMax(2, m_candleW - 1);
        m_gap = qMax(0, m_gap - 1);
    }
    m_crosshair = false;
    update();
}

void KLineChart::mousePressEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastPos = ev->pos();
        m_crosshair = false;
    }
}

void KLineChart::mouseMoveEvent(QMouseEvent *ev)
{
    if (m_dragging) {
        int dx = ev->pos().x() - m_lastPos.x();
        int cw = candleWidth();
        if (cw > 0 && qAbs(dx) >= cw) {
            m_scrollOffset = qBound(0, m_scrollOffset + dx / cw,
                                     qMax(0, m_data.size() - 1));
            m_lastPos = ev->pos();
            update();
        }
        return;
    }

    // 十字光标
    int idx = idxAtPos(ev->pos().x());
    if (idx >= 0 && idx < m_data.size()) {
        m_crosshair = true;
        m_crossIdx = idx;
        m_crossPos = ev->pos();
    } else {
        m_crosshair = false;
    }
    update();
}

void KLineChart::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

void KLineChart::resizeEvent(QResizeEvent *)
{
    update();
}
