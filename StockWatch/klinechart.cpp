#include "klinechart.h"
#include "indicator.h"

#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QHash>
#include <QtMath>
#include <algorithm>

// ============= 常量定义 =============
constexpr double KLineChart::K_MIN_W;
constexpr double KLineChart::K_MAX_W;
const int KLineChart::MIN_MAIN_H;
const int KLineChart::MIN_SUB_H;
const int KLineChart::SPLIT_HANDLE_H;

// ============= 颜色 =============
const QColor KLineChart::C_UP    = QColor(0xff, 0x4d, 0x5e);
const QColor KLineChart::C_DOWN  = QColor(0x2e, 0xe6, 0xa8);
const QColor KLineChart::C_BG    = QColor(0x0d, 0x14, 0x22);
const QColor KLineChart::C_GRID  = QColor(0x16, 0x22, 0x38);
const QColor KLineChart::C_TEXT  = QColor(0x8f, 0xa3, 0xc0);
const QColor KLineChart::C_CROSS = QColor(0x4d, 0x6b, 0x99);

const QColor KLineInfoPanel::C_UP   = KLineChart::C_UP;
const QColor KLineInfoPanel::C_DOWN = KLineChart::C_DOWN;
const QColor KLineInfoPanel::C_FLAT = KLineChart::C_TEXT;

// ============= 均线颜色 =============
static const QColor MA5_COLOR (0xff, 0xc1, 0x07);
static const QColor MA10_COLOR(0xe0, 0x40, 0xfb);
static const QColor MA20_COLOR(0x00, 0xe5, 0xff);
static const QColor MA60_COLOR(0xff, 0x91, 0x00);

// ============= BOLL 颜色 =============
static const QColor BOLL_UP_COLOR  (0xa0, 0x4d, 0x5e);
static const QColor BOLL_MID_COLOR (0xff, 0xc1, 0x07);
static const QColor BOLL_DOWN_COLOR(0x2e, 0xa6, 0x88);

// ============= MACD 颜色 =============
static const QColor MACD_DIF_COLOR(0xff, 0xff, 0xff);
static const QColor MACD_DEA_COLOR(0xff, 0x91, 0x00);

// ============= KDJ 颜色 =============
static const QColor KDJ_K_COLOR(0xff, 0xc1, 0x07);
static const QColor KDJ_D_COLOR(0xe0, 0x40, 0xfb);
static const QColor KDJ_J_COLOR(0x00, 0xe5, 0xff);

// ============= 成交量均线颜色 =============
// 与主图均线区分：量能均线用柔和色，避免与红绿量柱冲突
static const QColor VOL_MA5_COLOR (0xff, 0xd5, 0x4f);
static const QColor VOL_MA10_COLOR(0xc7, 0x9b, 0xff);
static const QColor VOL_MA20_COLOR(0x6d, 0xd4, 0xe8);

// ============= 字体 =============
static QFont axisFont()     { QFont f("Consolas", 8); return f; }
static QFont timeFont()     { QFont f("Consolas", 7); return f; }
static QFont legendFont()   { QFont f("Microsoft YaHei", 7); return f; }

// =====================================================================
// KLineChart 构造
// =====================================================================
KLineChart::KLineChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(300, 380);
    setStyleSheet(QString("background:%1;").arg(C_BG.name()));
}

void KLineChart::setData(const QVector<KLineData> &data)
{
    m_data = data;
    m_scrollOffset = 0;
    m_crosshair = false;
    m_hoverIdx = data.size() - 1;
    computeIndicatorSeries();
    update();
}

void KLineChart::clear()
{
    m_data.clear();
    m_ma5.clear(); m_ma10.clear(); m_ma20.clear(); m_ma60.clear();
    m_bollUp.clear(); m_bollMid.clear(); m_bollDown.clear();
    m_macdDIF.clear(); m_macdDEA.clear(); m_macdBar.clear();
    m_kdjK.clear(); m_kdjD.clear(); m_kdjJ.clear();
    m_flowNet.clear(); m_flowRatio.clear();
    m_crosshair = false;
    m_hoverIdx = -1;
    update();
}

const KLineData &KLineChart::hoveredOrLatest() const
{
    static KLineData empty;
    if (m_data.isEmpty()) return empty;
    if (m_hoverIdx >= 0 && m_hoverIdx < m_data.size())
        return m_data[m_hoverIdx];
    return m_data.last();
}

void KLineChart::setMainIndicator(MainIndicatorType ind)
{
    if (m_mainIndicator != ind) {
        m_mainIndicator = ind;
        update();
    }
}

void KLineChart::setSubIndicator(SubIndicatorType ind)
{
    if (m_subIndicator != ind) {
        m_subIndicator = ind;
        update();
    }
}

void KLineChart::setSplitRatio(double ratio)
{
    ratio = qBound(0.2, ratio, 0.85);
    if (qAbs(ratio - m_splitRatio) > 0.001) {
        m_splitRatio = ratio;
        emit splitRatioChanged(m_splitRatio);
        update();
    }
}

// =====================================================================
// 指示器序列计算（在 setData 中调用一次）
// =====================================================================
void KLineChart::computeIndicatorSeries()
{
    int n = m_data.size();
    if (n == 0) return;

    // MA 序列
    m_ma5.resize(n);  m_ma10.resize(n); m_ma20.resize(n); m_ma60.resize(n);

    auto computeMA = [&](QVector<double> &out, int period) {
        if (n < period) return;
        for (int i = 0; i < n; ++i) {
            if (i < period - 1) { out[i] = 0; continue; }
            double sum = 0;
            for (int t = i - period + 1; t <= i; ++t) sum += m_data[t].close;
            out[i] = sum / period;
        }
    };
    computeMA(m_ma5, 5);
    computeMA(m_ma10, 10);
    computeMA(m_ma20, 20);
    computeMA(m_ma60, 60);

    // BOLL / MACD / KDJ
    IndicatorEngine::bollSeries(m_data, m_bollUp, m_bollMid, m_bollDown);
    IndicatorEngine::macdSeries(m_data, m_macdDIF, m_macdDEA, m_macdBar);
    IndicatorEngine::kdjSeries(m_data, m_kdjK, m_kdjD, m_kdjJ);

    // 成交量均线（用于判断放量/缩量）
    auto computeVolMA = [&](QVector<double> &out, int period) {
        out.resize(n);
        out.fill(0.0);
        if (n < period) return;
        double sum = 0;
        for (int i = 0; i < n; ++i) {
            sum += m_data[i].volume;
            if (i >= period) sum -= m_data[i - period].volume;
            if (i >= period - 1) out[i] = sum / period;
        }
    };
    computeVolMA(m_volMa5, 5);
    computeVolMA(m_volMa10, 10);
    computeVolMA(m_volMa20, 20);
}

// =====================================================================
// 布局计算
// =====================================================================
int KLineChart::visibleCount() const
{
    if (m_candleW <= 0) return 1;
    int chartW = width() - L_MARGIN - R_PADDING - AXIS_W;
    return qMax(1, (int)(chartW / m_candleW));
}

int KLineChart::visibleStart() const
{
    return qMax(0, m_data.size() - m_scrollOffset - visibleCount());
}

double KLineChart::candleCenterX(int visIdx) const
{
    return L_MARGIN + visIdx * m_candleW + m_candleW / 2.0;
}

int KLineChart::idxAtX(int x) const
{
    int start = visibleStart();
    int idx = start + (int)((x - (double)L_MARGIN) / m_candleW);
    if (idx < 0 || idx >= m_data.size()) return -1;
    return idx;
}

void KLineChart::calcLayout()
{
    int w = width(), h = height();
    int totalChartH = h - T_MARGIN - B_MARGIN - TIME_H;

    int mainH = qMax(MIN_MAIN_H, (int)(totalChartH * m_splitRatio));
    int subH  = totalChartH - mainH;
    if (subH < MIN_SUB_H) {
        subH = MIN_SUB_H;
        mainH = totalChartH - subH;
    }

    int chartW = w - L_MARGIN - R_PADDING - AXIS_W;

    mainPlotRect  = QRect(L_MARGIN, T_MARGIN, chartW, mainH);
    priceAxisRect = QRect(L_MARGIN + chartW + R_PADDING, T_MARGIN, AXIS_W, mainH);

    subPlotRect  = QRect(L_MARGIN, T_MARGIN + mainH, chartW, subH);
    subAxisRect  = QRect(L_MARGIN + chartW + R_PADDING, T_MARGIN + mainH, AXIS_W, subH);

    timeAxisRect = QRect(L_MARGIN, T_MARGIN + mainH + subH, chartW, TIME_H);

    m_splitHandleRect = QRect(L_MARGIN, mainPlotRect.bottom() + 1,
                              chartW, SPLIT_HANDLE_H);

    // 更新当前比例（用于下拉后发射信号）
    m_splitRatio = (double)mainH / totalChartH;
}

// =====================================================================
// paintEvent
// =====================================================================
void KLineChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    calcLayout();

    if (m_data.isEmpty()) {
        p.setPen(C_TEXT);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("加载中…"));
        return;
    }

    drawChart(p);
}

// =====================================================================
// 绘制总调度
// =====================================================================
void KLineChart::drawChart(QPainter &p)
{
    // 1. 主图：网格 + K线 + 主图指示器
    drawMainGrid(p);
    drawCandles(p);

    if (m_mainIndicator == MainIndicatorType::MA)
        drawMovingAverages(p);
    else if (m_mainIndicator == MainIndicatorType::BOLL)
        drawBollinger(p);

    // 2. 副图：按类型调度
    if (m_subIndicator == SubIndicatorType::Volume)
        drawVolumeBars(p);
    else if (m_subIndicator == SubIndicatorType::MACD)
        drawMACD(p);
    else if (m_subIndicator == SubIndicatorType::KDJ)
        drawKDJ(p);
    else if (m_subIndicator == SubIndicatorType::MoneyFlow)
        drawMoneyFlow(p);

    // 3. 时间轴 + 分隔线 + 图表侧边栏竖线 + 十字光标
    drawTimeAxis(p);
    drawSepLine(p);
    drawPanelBorder(p);
    if (m_crosshair && m_hoverIdx >= 0)
        drawCrosshair(p);

    // 4. 最后绘制坐标轴文字，确保不被任何图形覆盖
    drawPriceAxis(p);
    drawSubAxis(p);

    // 5. 坐标轴浮标必须画在轴文字之后，否则会被轴标签遮住
    if (m_crosshair && m_hoverIdx >= 0)
        drawCrosshairTags(p);
}

// =====================================================================
// 主图网格
// =====================================================================
void KLineChart::drawMainGrid(QPainter &p)
{
    p.save();
    p.setClipRect(mainPlotRect);

    double maxP = 0, minP = 1e15;
    int start = visibleStart(), vis = visibleCount();
    for (int i = start; i < m_data.size() && i < start + vis; ++i) {
        if (m_data[i].high > maxP) maxP = m_data[i].high;
        if (m_data[i].low  < minP) minP = m_data[i].low;
    }
    if (maxP == minP) { maxP += 0.1; minP -= 0.1; }
    double margin = (maxP - minP) * 0.04;
    maxP += margin; minP -= margin;

    auto ticks = niceTicks(minP, maxP, 8);
    p.setPen(QPen(C_GRID, 1, Qt::DotLine));
    for (double t : ticks) {
        int y = (int)(mainPlotRect.bottom() -
                      (t - minP) / (maxP - minP) * mainPlotRect.height());
        if (y >= mainPlotRect.top() && y <= mainPlotRect.bottom())
            p.drawLine(mainPlotRect.left(), y, mainPlotRect.right(), y);
    }
    p.restore();
}

// =====================================================================
// 获取主图可见价格范围和 yPos 映射函数
// =====================================================================
static void mainPriceRange(const KLineChart *chart,
                           const QVector<KLineData> &data,
                           double &minP, double &maxP)
{
    maxP = 0; minP = 1e15;
    int start = chart->visibleStart(), vis = chart->visibleCount();
    for (int i = start; i < data.size() && i < start + vis; ++i) {
        if (data[i].high > maxP) maxP = data[i].high;
        if (data[i].low  < minP) minP = data[i].low;
    }
    if (maxP == minP) { maxP += 0.1; minP -= 0.1; }
    double m = (maxP - minP) * 0.04;
    maxP += m; minP -= m;
}

// =====================================================================
// K线
// =====================================================================
void KLineChart::drawCandles(QPainter &p)
{
    p.save();
    p.setClipRect(mainPlotRect);

    int start = visibleStart(), vis = visibleCount();

    double maxP, minP;
    mainPriceRange(this, m_data, minP, maxP);

    auto yPos = [&](double price) {
        return mainPlotRect.bottom() - (price - minP) / (maxP - minP) * mainPlotRect.height();
    };

    double hw = qMax(0.5, m_candleW * 0.38);
    double bodyW = m_candleW * 0.76;

    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_data.size()) break;
        const auto &d = m_data[i];
        double cx = candleCenterX(cnt);
        bool up = d.close >= d.open;
        QColor c = up ? C_UP : C_DOWN;

        double yc = yPos(d.close);
        double yo = yPos(d.open);
        double yh = yPos(d.high);
        double yl = yPos(d.low);

        // 影线
        p.setPen(QPen(c, 1.0));
        p.drawLine(QPointF(cx, yh), QPointF(cx, yl));

        // 实体
        double top = qMin(yc, yo);
        double hh  = qAbs(yc - yo);
        if (hh < 1.0) hh = 1.0;
        p.fillRect(QRectF(cx - hw, top, bodyW, hh), c);
    }
    p.restore();
}

// =====================================================================
// 均线（使用缓存序列）
// =====================================================================
void KLineChart::drawMovingAverages(QPainter &p)
{
    p.save();
    p.setClipRect(mainPlotRect);

    int vis = visibleCount(), start = visibleStart();
    if (vis < 2) { p.restore(); return; }

    double maxP, minP;
    mainPriceRange(this, m_data, minP, maxP);

    auto yPos = [&](double v) {
        return mainPlotRect.bottom() - (v - minP) / (maxP - minP) * mainPlotRect.height();
    };

    auto drawLine = [&](const QVector<double> &ma, const QColor &color) {
        QPen pen(color, 1.2);
        drawIndicatorLine(p, ma, start, vis, yPos, pen);
    };

    drawLine(m_ma5,  MA5_COLOR);
    drawLine(m_ma10, MA10_COLOR);
    drawLine(m_ma20, MA20_COLOR);
    drawLine(m_ma60, MA60_COLOR);

    // 图例
    p.setFont(legendFont());
    int legendX = mainPlotRect.left() + 4;
    int legendY = mainPlotRect.top() + 4;
    int legendW = 0;
    auto legend = [&](const QString &txt, const QColor &c) {
        p.setPen(c);
        int tw = p.fontMetrics().width(txt) + 6;
        p.fillRect(legendX + legendW - 2, legendY, tw, 16, QColor(0x0d, 0x14, 0x22, 180));
        p.drawText(legendX + legendW + 1, legendY, tw, 16, Qt::AlignLeft | Qt::AlignVCenter, txt);
        legendW += tw + 4;
    };
    legend("MA5",  MA5_COLOR);
    legend("MA10", MA10_COLOR);
    legend("MA20", MA20_COLOR);
    legend("MA60", MA60_COLOR);

    p.restore();
}

// =====================================================================
// 布林带
// =====================================================================
void KLineChart::drawBollinger(QPainter &p)
{
    p.save();
    p.setClipRect(mainPlotRect);

    int vis = visibleCount(), start = visibleStart();
    if (vis < 2 || m_data.isEmpty()) { p.restore(); return; }

    double maxP, minP;
    mainPriceRange(this, m_data, minP, maxP);

    // 也考虑布林带值扩展范围
    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_data.size()) break;
        if (i < m_bollUp.size() && m_bollUp[i] > 0) {
            if (m_bollUp[i]   > maxP) maxP = m_bollUp[i];
            if (m_bollDown[i] < minP) minP = m_bollDown[i];
        }
    }
    double m = (maxP - minP) * 0.04;
    maxP += m; minP -= m;

    auto yPos = [&](double v) {
        return mainPlotRect.bottom() - (v - minP) / (maxP - minP) * mainPlotRect.height();
    };

    QPen upPen(BOLL_UP_COLOR, 1.0);
    QPen midPen(BOLL_MID_COLOR, 1.0);
    QPen downPen(BOLL_DOWN_COLOR, 1.0);

    drawIndicatorLine(p, m_bollUp,   start, vis, yPos, upPen);
    drawIndicatorLine(p, m_bollMid,  start, vis, yPos, midPen);
    drawIndicatorLine(p, m_bollDown, start, vis, yPos, downPen);

    // 图例
    p.setFont(legendFont());
    int lx = mainPlotRect.left() + 4, ly = mainPlotRect.top() + 4;
    p.setPen(BOLL_MID_COLOR);
    p.fillRect(lx - 2, ly, 80, 16, QColor(0x0d, 0x14, 0x22, 180));
    p.drawText(lx + 1, ly, 78, 16, Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("BOLL(20,2)"));

    p.restore();
}

// =====================================================================
// 成交量
// =====================================================================
void KLineChart::drawVolumeBars(QPainter &p)
{
    p.save();
    p.setClipRect(subPlotRect);

    int start = visibleStart(), vis = visibleCount();
    if (vis <= 0) { p.restore(); return; }

    // 量能上限：同时考虑成交量柱与均线，避免均线被裁
    double maxV = 0;
    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_data.size()) break;
        maxV = qMax(maxV, m_data[i].volume);
        if (i < m_volMa20.size()) maxV = qMax(maxV, m_volMa20[i]);
    }
    if (maxV <= 0) maxV = 1;
    maxV *= 1.06;   // 顶部留白

    double hw = qMax(0.5, m_candleW * 0.38);
    double bodyW = m_candleW * 0.76;

    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_data.size()) break;
        const auto &d = m_data[i];
        bool up = d.close >= d.open;
        QColor c = up ? C_UP : C_DOWN;
        c.setAlpha(140);

        double vh = d.volume / maxV * subPlotRect.height();
        if (vh < 1.0) vh = 1.0;
        double cx = candleCenterX(cnt);
        p.fillRect(QRectF(cx - hw, subPlotRect.bottom() - vh, bodyW, vh), c);
    }

    // ---- 成交量均线（MA5 / MA10 / MA20）----
    auto volY = [&](double v) {
        return subPlotRect.bottom() - v / maxV * subPlotRect.height();
    };
    drawIndicatorLine(p, m_volMa5,  start, vis, volY, QPen(VOL_MA5_COLOR, 1.2));
    drawIndicatorLine(p, m_volMa10, start, vis, volY, QPen(VOL_MA10_COLOR, 1.2));
    drawIndicatorLine(p, m_volMa20, start, vis, volY, QPen(VOL_MA20_COLOR, 1.2));

    // ---- 图例：显示"当前关注"那根K线的量能 ----
    // 十字光标激活时跟随光标所指的那根，否则显示最右侧（最新）一根。
    // 早期固定用最右侧，导致光标停在历史K线时，图例与右侧面板数据不一致。
    p.setFont(legendFont());
    auto fmtVolL = [](double v) -> QString {
        if (v >= 1e8) return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
        if (v >= 1e4) return QString::number(v / 1e4, 'f', 0) + QStringLiteral("万");
        return QString::number(v, 'f', 0);
    };
    int lx = subPlotRect.left() + 4;
    int ly = subPlotRect.top() + 2;
    auto legend = [&](const QString &txt, const QColor &c) {
        p.setPen(c);
        const int tw = p.fontMetrics().width(txt) + 6;
        p.drawText(lx, ly, tw, 14, Qt::AlignLeft, txt);
        lx += tw;
    };
    int legendIdx = qMin(m_data.size() - 1, start + vis - 1);
    if (m_crosshair && m_hoverIdx >= 0 && m_hoverIdx < m_data.size())
        legendIdx = m_hoverIdx;
    if (legendIdx >= 0) {
        const double vol = m_data[legendIdx].volume;
        const double ma5 = (legendIdx < m_volMa5.size()) ? m_volMa5[legendIdx] : 0;
        // 量比：相对 5 日均量，辅助判断放量/缩量
        QString ratioTxt;
        QColor ratioColor = C_TEXT;
        if (ma5 > 0) {
            const double ratio = vol / ma5;
            ratioTxt = QStringLiteral(" 量比 %1").arg(ratio, 0, 'f', 2);
            if (ratio >= 1.5)      ratioColor = C_UP;      // 放量
            else if (ratio <= 0.6) ratioColor = C_DOWN;    // 缩量
        }
        p.setPen(C_TEXT);
        QString t = QStringLiteral("成交量 ") + fmtVolL(vol);
        int tw = p.fontMetrics().width(t) + 6;
        p.drawText(lx, ly, tw, 14, Qt::AlignLeft, t);
        lx += tw;
        if (!ratioTxt.isEmpty()) {
            p.setPen(ratioColor);
            p.drawText(lx, ly, p.fontMetrics().width(ratioTxt) + 6, 14,
                       Qt::AlignLeft, ratioTxt);
            lx += p.fontMetrics().width(ratioTxt) + 6;
        }
    }
    // 均线图例带数值：直接读出当前这根的量能均线水平
    auto volMaAt = [&](const QVector<double> &s) -> QString {
        if (legendIdx < 0 || legendIdx >= s.size() || s[legendIdx] <= 0) return QString();
        return fmtVolL(s[legendIdx]);
    };
    legend(volMaAt(m_volMa5).isEmpty()  ? QStringLiteral("MA5")
                                        : QStringLiteral("MA5 ") + volMaAt(m_volMa5),  VOL_MA5_COLOR);
    legend(volMaAt(m_volMa10).isEmpty() ? QStringLiteral("MA10")
                                        : QStringLiteral("MA10 ") + volMaAt(m_volMa10), VOL_MA10_COLOR);
    legend(volMaAt(m_volMa20).isEmpty() ? QStringLiteral("MA20")
                                        : QStringLiteral("MA20 ") + volMaAt(m_volMa20), VOL_MA20_COLOR);

    p.restore();
}

// =====================================================================
// 资金流数据（按日期对齐到K线）
// =====================================================================
void KLineChart::setMoneyFlow(const QVector<FlowPoint> &flows)
{
    m_flowNet.clear();
    m_flowRatio.clear();
    if (flows.isEmpty() || m_data.isEmpty()) { update(); return; }

    QHash<QString, int> idxOfDate;
    idxOfDate.reserve(flows.size());
    for (int i = 0; i < flows.size(); ++i)
        idxOfDate.insert(flows[i].date.left(10), i);

    const int n = m_data.size();
    m_flowNet.resize(n);
    m_flowRatio.resize(n);
    int matched = 0;
    for (int i = 0; i < n; ++i) {
        const int fi = idxOfDate.value(m_data[i].date.left(10), -1);
        if (fi >= 0) {
            m_flowNet[i]   = flows[fi].mainNet;
            m_flowRatio[i] = flows[fi].mainRatio;
            ++matched;
        } else {
            m_flowNet[i]   = 0;
            m_flowRatio[i] = 0;
        }
    }
    // 完全无法匹配(如分钟线周期)时清空，避免显示误导性空图
    if (matched == 0) { m_flowNet.clear(); m_flowRatio.clear(); }
    update();
}

// =====================================================================
// 资金流副图：主力净流入柱 + 零轴
// =====================================================================
void KLineChart::drawMoneyFlow(QPainter &p)
{
    p.save();
    p.setClipRect(subPlotRect);

    int start = visibleStart(), vis = visibleCount();
    if (vis <= 0) { p.restore(); return; }

    if (m_flowNet.isEmpty()) {
        p.setFont(legendFont());
        p.setPen(C_TEXT);
        p.drawText(subPlotRect, Qt::AlignCenter,
                   QStringLiteral("暂无主力资金流数据（分钟线周期不支持）"));
        p.restore();
        return;
    }

    double maxV = 0, minV = 0;
    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_flowNet.size()) break;
        maxV = qMax(maxV, m_flowNet[i]);
        minV = qMin(minV, m_flowNet[i]);
    }
    if (maxV == 0 && minV == 0) maxV = 1;
    if (maxV == minV) { maxV += 1; minV -= 1; }
    const double mrg = (maxV - minV) * 0.08;
    maxV += mrg; minV -= mrg;

    auto yPos = [&](double v) {
        return subPlotRect.bottom() - (v - minV) / (maxV - minV) * subPlotRect.height();
    };

    const int zy = (int)yPos(0);
    p.setPen(QPen(C_GRID, 1, Qt::DotLine));
    p.drawLine(subPlotRect.left(), zy, subPlotRect.right(), zy);

    const double hw = qMax(0.5, m_candleW * 0.38);
    const double bodyW = m_candleW * 0.76;
    const QColor inColor (0xff, 0x4d, 0x5e, 190);
    const QColor outColor(0x2e, 0xe6, 0xa8, 190);

    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_flowNet.size()) break;
        const double v = m_flowNet[i];
        const QColor c = (v >= 0) ? inColor : outColor;
        const double yv = yPos(v);
        double barH = qAbs(yv - zy);
        if (barH < 1.0) barH = 1.0;
        const double cx = candleCenterX(cnt);
        p.fillRect(QRectF(cx - hw, qMin(zy, (int)yv), bodyW, barH), c);
    }

    // 图例：可见区间主力净流入合计
    p.setFont(legendFont());
    double sum = 0;
    int counted = 0;
    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_flowNet.size()) break;
        sum += m_flowNet[i];
        ++counted;
    }
    auto fmtMoney = [](double v) -> QString {
        const double a = qAbs(v);
        if (a >= 1e8) return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
        if (a >= 1e4) return QString::number(v / 1e4, 'f', 0) + QStringLiteral("万");
        return QString::number(v, 'f', 0);
    };
    int lx = subPlotRect.left() + 4;
    const QString t1 = QStringLiteral("主力净流入 %1%2")
                           .arg(sum >= 0 ? "+" : "").arg(fmtMoney(sum));
    p.setPen((sum >= 0) ? C_UP : C_DOWN);
    p.drawText(lx, subPlotRect.top() + 2,
               p.fontMetrics().width(t1) + 4, 14, Qt::AlignLeft, t1);
    lx += p.fontMetrics().width(t1) + 8;
    p.setPen(C_TEXT);
    p.drawText(lx, subPlotRect.top() + 2, 200, 14, Qt::AlignLeft,
               QStringLiteral("(可见%1日)").arg(counted));

    p.restore();
}

// =====================================================================
// MACD
// =====================================================================
void KLineChart::drawMACD(QPainter &p)
{
    p.save();
    p.setClipRect(subPlotRect);

    int start = visibleStart(), vis = visibleCount();
    if (vis <= 0) { p.restore(); return; }

    // 计算可见范围极值
    double minV = 1e15, maxV = -1e15;
    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_data.size()) break;
        bool has = false;
        auto check = [&](double v) { if (v != 0 || has) { minV = qMin(minV, v); maxV = qMax(maxV, v); has = true; } };
        check(m_macdDIF[i]); check(m_macdDEA[i]); check(m_macdBar[i]);
    }
    if (minV >= maxV) { maxV = minV + 1; minV -= 1; }
    double margin = (maxV - minV) * 0.05;
    maxV += margin; minV -= margin;
    if (minV > 0) minV = -margin;
    if (maxV < 0) maxV = margin;

    auto yPos = [&](double v) {
        return subPlotRect.bottom() - (v - minV) / (maxV - minV) * subPlotRect.height();
    };

    // 零线
    int zy = (int)yPos(0);
    p.setPen(QPen(C_GRID, 1, Qt::DotLine));
    p.drawLine(subPlotRect.left(), zy, subPlotRect.right(), zy);

    // 柱状图
    double hw = qMax(0.5, m_candleW * 0.38);
    double bodyW = m_candleW * 0.76;
    QColor barUp  (0xff, 0x4d, 0x5e, 180);
    QColor barDown(0x2e, 0xe6, 0xa8, 180);

    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_data.size()) break;
        double barVal = m_macdBar[i];
        QColor c = barVal >= 0 ? barUp : barDown;
        double yv = yPos(barVal);
        double barH = qAbs(yv - zy);
        if (barH < 1) barH = 1;
        double cx = candleCenterX(cnt);
        p.fillRect(QRectF(cx - hw, qMin(zy, (int)yv), bodyW, barH), c);
    }

    // DIF / DEA 线
    QPen difPen(MACD_DIF_COLOR, 1.2);
    QPen deaPen(MACD_DEA_COLOR, 1.2);
    drawIndicatorLine(p, m_macdDIF, start, vis, yPos, difPen);
    drawIndicatorLine(p, m_macdDEA, start, vis, yPos, deaPen);

    // 图例
    p.setFont(legendFont());
    auto legend = [&](int &x, const QString &txt, const QColor &c) {
        p.setPen(c);
        int tw = p.fontMetrics().width(txt) + 4;
        p.drawText(x, subPlotRect.top() + 2, tw, 14, Qt::AlignLeft, txt);
        x += tw;
    };
    int lx = subPlotRect.left() + 4;
    legend(lx, "DIF", MACD_DIF_COLOR);
    legend(lx, "DEA", MACD_DEA_COLOR);
    legend(lx, "MACD(12,26,9)", C_TEXT);

    p.restore();
}

// =====================================================================
// KDJ
// =====================================================================
void KLineChart::drawKDJ(QPainter &p)
{
    p.save();
    p.setClipRect(subPlotRect);

    int start = visibleStart(), vis = visibleCount();
    if (vis <= 0) { p.restore(); return; }

    double minV = 0, maxV = 100;
    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_data.size()) break;
        if (m_kdjK[i] != 0 || m_kdjD[i] != 0 || m_kdjJ[i] != 0) {
            minV = qMin(minV, qMin(m_kdjK[i], qMin(m_kdjD[i], m_kdjJ[i])));
            maxV = qMax(maxV, qMax(m_kdjK[i], qMax(m_kdjD[i], m_kdjJ[i])));
        }
    }
    double margin = (maxV - minV) * 0.05;
    maxV += margin; minV -= margin;

    auto yPos = [&](double v) {
        return subPlotRect.bottom() - (v - minV) / (maxV - minV) * subPlotRect.height();
    };

    // 参考线 20 / 50 / 80
    p.setPen(QPen(C_GRID, 1, Qt::DotLine));
    for (double ref : {20.0, 50.0, 80.0}) {
        if (ref >= minV && ref <= maxV) {
            int ry = (int)yPos(ref);
            p.drawLine(subPlotRect.left(), ry, subPlotRect.right(), ry);
        }
    }

    QPen kPen(KDJ_K_COLOR, 1.0);
    QPen dPen(KDJ_D_COLOR, 1.0);
    QPen jPen(KDJ_J_COLOR, 1.0);

    drawIndicatorLine(p, m_kdjK, start, vis, yPos, kPen);
    drawIndicatorLine(p, m_kdjD, start, vis, yPos, dPen);
    drawIndicatorLine(p, m_kdjJ, start, vis, yPos, jPen);

    // 图例
    p.setFont(legendFont());
    auto legend = [&](int &x, const QString &txt, const QColor &c) {
        p.setPen(c);
        int tw = p.fontMetrics().width(txt) + 4;
        p.drawText(x, subPlotRect.top() + 2, tw, 14, Qt::AlignLeft, txt);
        x += tw;
    };
    int lx = subPlotRect.left() + 4;
    legend(lx, "K", KDJ_K_COLOR);
    legend(lx, "D", KDJ_D_COLOR);
    legend(lx, "J", KDJ_J_COLOR);

    p.restore();
}

// =====================================================================
// 价格轴
// =====================================================================
void KLineChart::drawPriceAxis(QPainter &p)
{
    p.save();

    int start = visibleStart(), vis = visibleCount();
    double maxP = 0, minP = 1e15;
    for (int i = start; i < m_data.size() && i < start + vis; ++i) {
        if (m_data[i].high > maxP) maxP = m_data[i].high;
        if (m_data[i].low  < minP) minP = m_data[i].low;
    }
    if (maxP == minP) { maxP += 0.1; minP -= 0.1; }
    double margin = (maxP - minP) * 0.04;
    maxP += margin; minP -= margin;

    auto ticks = niceTicks(minP, maxP, 8);

    p.setPen(C_TEXT);
    p.setFont(axisFont());
    QFontMetrics fm(p.font());
    int textH = fm.height();
    int halfH = textH / 2;

    p.setClipRect(priceAxisRect.adjusted(0, -halfH, 0, halfH));

    for (double t : ticks) {
        double frac = (t - minP) / (maxP - minP);
        int y = (int)(priceAxisRect.bottom() - frac * priceAxisRect.height());
        QString label = QString::number(t, 'f', 2);
        int lw = fm.width(label);
        // 夹紧垂直位置：顶部/底部的刻度若居中会有一半被裁掉（只显示半截数字）
        int ty = y - halfH;
        if (ty < priceAxisRect.top())            ty = priceAxisRect.top();
        if (ty > priceAxisRect.bottom() - textH) ty = priceAxisRect.bottom() - textH;
        QRect textR(priceAxisRect.right() - lw - 2, ty, lw, textH);
        p.drawText(textR, Qt::AlignRight | Qt::AlignVCenter, label);
    }
    p.restore();
}

// =====================================================================
// 副图轴调度
// =====================================================================
void KLineChart::drawSubAxis(QPainter &p)
{
    if (m_subIndicator == SubIndicatorType::Volume)
        drawVolumeAxisContent(p);
    else if (m_subIndicator == SubIndicatorType::MoneyFlow)
        drawFlowAxisContent(p);
    else
        drawIndicatorAxisContent(p);
}

// 资金流副图坐标轴：万/亿 中文单位
void KLineChart::drawFlowAxisContent(QPainter &p)
{
    p.save();
    p.setFont(axisFont());
    QFontMetrics fm(p.font());
    const int textH = fm.height();
    const int halfH = textH / 2;
    p.setClipRect(subAxisRect.adjusted(0, -halfH, 0, halfH));
    p.setPen(C_TEXT);

    if (m_flowNet.isEmpty()) {
        p.drawText(QRect(subAxisRect.left(), subAxisRect.center().y() - textH / 2,
                         subAxisRect.width(), textH),
                   Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("--"));
        p.restore();
        return;
    }

    int start = visibleStart(), vis = visibleCount();
    double maxV = 0, minV = 0;
    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= m_flowNet.size()) break;
        maxV = qMax(maxV, m_flowNet[i]);
        minV = qMin(minV, m_flowNet[i]);
    }
    if (maxV == 0 && minV == 0) { maxV = 1e8; minV = -1e8; }
    if (maxV == minV) { maxV += 1e6; minV -= 1e6; }
    const double mrg = (maxV - minV) * 0.08;
    maxV += mrg; minV -= mrg;

    auto fmtMoney = [](double v) -> QString {
        const double a = qAbs(v);
        if (a >= 1e8) return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
        if (a >= 1e4) return QString::number(v / 1e4, 'f', 1) + QStringLiteral("万");
        return QString::number(v, 'f', 0);
    };

    const int ticks = qMin(4, qMax(3, subAxisRect.height() / 26));
    for (int i = 0; i <= ticks; ++i) {
        const double v = minV + (maxV - minV) * i / ticks;
        const int y = subAxisRect.bottom() - subAxisRect.height() * i / ticks;
        const QString label = fmtMoney(v);
        const int lw = fm.width(label);
        p.drawText(QRect(subAxisRect.right() - lw - 2, y - halfH, lw, textH),
                   Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // 零轴标签（若在可见范围内）
    if (minV < 0 && maxV > 0) {
        const int zy = (int)(subAxisRect.bottom() - (0 - minV) / (maxV - minV) * subAxisRect.height());
        const QString zl = QStringLiteral("0");
        const int lw = fm.width(zl);
        p.drawText(QRect(subAxisRect.right() - lw - 2, zy - halfH, lw, textH),
                   Qt::AlignRight | Qt::AlignVCenter, zl);
    }
    p.restore();
}

void KLineChart::drawVolumeAxisContent(QPainter &p)
{
    p.save();

    p.setFont(axisFont());
    QFontMetrics fm(p.font());
    int textH = fm.height();
    int halfH = textH / 2;

    p.setClipRect(subAxisRect.adjusted(0, -halfH, 0, halfH));

    int start = visibleStart(), vis = visibleCount();
    double maxV = 0;
    for (int i = start; i < m_data.size() && i < start + vis; ++i)
        if (m_data[i].volume > maxV) maxV = m_data[i].volume;
    if (maxV == 0) maxV = 1;

    {
        double mag = 1, mm = maxV / 4;
        while (mm >= 1e4) { mag *= 1e4; mm /= 1e4; }
        while (mm >= 10)  { mag *= 10;  mm /= 10; }
        maxV = mag * qCeil(mm);
    }

    int ticks = qMin(4, qMax(3, subAxisRect.height() / 26));
    p.setPen(C_TEXT);

    auto fmtVol = [](double v) -> QString {
        if (v >= 1e8) return QString::number(v / 1e8, 'f', 1) + QStringLiteral("亿");
        if (v >= 1e4) return QString::number(v / 1e4, 'f', 0) + QStringLiteral("万");
        return QString::number(v, 'f', 0);
    };

    for (int i = 0; i <= ticks; ++i) {
        double v = maxV * i / ticks;
        int y = subAxisRect.bottom() - subAxisRect.height() * i / ticks;
        QString label = fmtVol(v);
        int lw = fm.width(label);
        int ty = y - halfH;
        if (ty < subAxisRect.top())              ty = subAxisRect.top();
        if (ty > subAxisRect.bottom() - textH)   ty = subAxisRect.bottom() - textH;
        QRect textR(subAxisRect.right() - lw - 2, ty, lw, textH);
        p.drawText(textR, Qt::AlignRight | Qt::AlignVCenter, label);
    }
    p.restore();
}

void KLineChart::drawIndicatorAxisContent(QPainter &p)
{
    p.save();

    int start = visibleStart(), vis = visibleCount();
    double minV = 1e15, maxV = -1e15;

    if (m_subIndicator == SubIndicatorType::MACD) {
        for (int cnt = 0; cnt < vis; ++cnt) {
            int i = start + cnt;
            if (i >= m_data.size()) break;
            minV = qMin(minV, qMin(m_macdDIF[i], qMin(m_macdDEA[i], m_macdBar[i])));
            maxV = qMax(maxV, qMax(m_macdDIF[i], qMax(m_macdDEA[i], m_macdBar[i])));
        }
        if (minV > 0) minV = -0.01;
        if (maxV < 0) maxV = 0.01;
    } else {
        minV = 0; maxV = 100;
        for (int cnt = 0; cnt < vis; ++cnt) {
            int i = start + cnt;
            if (i >= m_data.size()) break;
            if (m_kdjK[i] != 0) {
                minV = qMin(minV, qMin(m_kdjK[i], qMin(m_kdjD[i], m_kdjJ[i])));
                maxV = qMax(maxV, qMax(m_kdjK[i], qMax(m_kdjD[i], m_kdjJ[i])));
            }
        }
    }

    if (minV >= maxV) { maxV = minV + 1; minV -= 1; }
    double mrg = (maxV - minV) * 0.05;
    maxV += mrg; minV -= mrg;

    p.setFont(axisFont());
    QFontMetrics fm(p.font());
    int textH = fm.height();
    int halfH = textH / 2;

    p.setClipRect(subAxisRect.adjusted(0, -halfH, 0, halfH));

    auto ticks = niceTicks(minV, maxV, 5);
    p.setPen(C_TEXT);

    for (double t : ticks) {
        double frac = (t - minV) / (maxV - minV);
        int y = (int)(subAxisRect.bottom() - frac * subAxisRect.height());
        // 根据数值范围选择精度
        QString label;
        double absRange = maxV - minV;
        if (absRange < 1)       label = QString::number(t, 'f', 3);
        else if (absRange < 10) label = QString::number(t, 'f', 2);
        else                    label = QString::number(t, 'f', 1);
        int lw = fm.width(label);
        QRect textR(subAxisRect.right() - lw - 2, y - halfH, lw, textH);
        p.drawText(textR, Qt::AlignRight | Qt::AlignVCenter, label);
    }
    p.restore();
}

// =====================================================================
// 时间轴
// =====================================================================
void KLineChart::drawTimeAxis(QPainter &p)
{
    p.save();
    p.setClipRect(timeAxisRect);

    int start = visibleStart(), vis = visibleCount();
    if (vis <= 0) { p.restore(); return; }

    p.setFont(timeFont());
    p.setPen(C_TEXT);
    QFontMetrics fm(p.font());

    int maxLabels = qMax(5, qMin(12, timeAxisRect.width() / (fm.width("2026-12-31") + 8)));
    int step = qMax(1, vis / maxLabels);

    int lastYear  = -1;
    int lastLabelRight = -999;

    for (int cnt = 0; cnt < vis; cnt += step) {
        int idx = start + cnt;
        if (idx >= m_data.size()) break;

        const QString &full = m_data[idx].date;
        int year = full.left(4).toInt();

        QString label;
        if (year != lastYear || (cnt == 0 && year != QDate::currentDate().year())) {
            label = full.left(10);
            lastYear = year;
        } else {
            label = full.right(5);
        }

        double cx = candleCenterX(cnt);
        int lw = fm.width(label);
        int lx = (int)cx - lw / 2;

        // 最左侧/最右侧的标签居中后会超出绘制区被 clip 裁掉（出现 "-04-24"），
        // 这里把标签夹紧到可见范围内，保证日期完整可读。
        const int minX = timeAxisRect.left();
        const int maxX = timeAxisRect.right() - lw;
        if (lx < minX) lx = minX;
        if (lx > maxX) lx = maxX;

        if (lx < lastLabelRight + 4) continue;
        lastLabelRight = lx + lw;

        p.drawText(QRect(lx, timeAxisRect.top(), lw, timeAxisRect.height()),
                   Qt::AlignCenter, label);
    }
    p.restore();
}

// =====================================================================
// 分割线
// =====================================================================
void KLineChart::drawSepLine(QPainter &p)
{
    p.save();
    p.setClipRect(QRect(L_MARGIN, 0, mainPlotRect.width(), height()));

    QPen mainSep(C_TEXT.darker(130), 2);
    p.setPen(mainSep);
    int y1 = mainPlotRect.bottom();
    p.drawLine(L_MARGIN, y1, mainPlotRect.right(), y1);

    QPen subSep(C_GRID, 1);
    p.setPen(subSep);
    int y2 = subPlotRect.bottom();
    p.drawLine(L_MARGIN, y2, subPlotRect.right(), y2);

    p.restore();
}

// =====================================================================
// 面板竖向分隔线 —— 从主图顶部贯穿到时间轴底部
// =====================================================================
void KLineChart::drawPanelBorder(QPainter &p)
{
    p.save();
    // 竖线位置：轴区域右侧边缘
    int x = priceAxisRect.right();
    int yTop = mainPlotRect.top();
    int yBot = timeAxisRect.bottom();

    QPen pen(C_GRID, 1);
    p.setPen(pen);
    p.drawLine(x, yTop, x, yBot);
    p.restore();
}

// =====================================================================
// 十字光标
// =====================================================================
void KLineChart::drawCrosshair(QPainter &p)
{
    p.save();
    QRect chartArea = mainPlotRect.united(subPlotRect);
    p.setClipRect(chartArea);

    int vis = visibleCount(), start = visibleStart();
    int cnt = m_hoverIdx - start;
    if (cnt < 0 || cnt >= vis) { p.restore(); return; }
    double cx = candleCenterX(cnt);

    QPen pen(C_CROSS, 1, Qt::DashLine);
    p.setPen(pen);

    p.drawLine(QPointF(cx, mainPlotRect.top()),
               QPointF(cx, subPlotRect.bottom()));

    int my = m_mousePos.y();
    bool inMain = mainPlotRect.contains(m_mousePos);
    bool inSub  = subPlotRect.contains(m_mousePos);
    if (inMain) {
        p.drawLine(mainPlotRect.left(), my, mainPlotRect.right(), my);
    } else if (inSub) {
        p.drawLine(subPlotRect.left(), my, subPlotRect.right(), my);
    }

    p.restore();
}

// 绘制十字光标在坐标轴上的数值浮标 + 时间轴日期浮标
// 注意：必须在 drawPriceAxis/drawSubAxis 之后调用，否则会被轴标签覆盖
void KLineChart::drawCrosshairTags(QPainter &p)
{
    p.save();
    p.setFont(axisFont());
    QFontMetrics fm(p.font());

    const int start = visibleStart();
    const int vis = visibleCount();
    const int cnt = m_hoverIdx - start;
    if (cnt < 0 || cnt >= vis) { p.restore(); return; }
    const int idx = m_hoverIdx;
    if (idx < 0 || idx >= m_data.size()) { p.restore(); return; }

    const int mouseY = m_mousePos.y();
    const bool inMain = mainPlotRect.contains(m_mousePos);
    const bool inSub  = subPlotRect.contains(m_mousePos);

    // 深色底 + 亮边的小标签
    auto drawTag = [&](const QRect &r, const QString &text, const QColor &fg) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x2a, 0x3d, 0x5c, 235));
        p.drawRoundedRect(r, 2, 2);
        p.setPen(QPen(QColor(0x4d, 0xa3, 0xff), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 2, 2);
        p.setPen(fg);
        p.drawText(r, Qt::AlignCenter, text);
    };

    // ---- 价格轴浮标 ----
    if (inMain && !m_data.isEmpty()) {
        // 复用主图价格范围计算逻辑
        double maxP = 0, minP = 1e15;
        for (int i = start; i < m_data.size() && i < start + vis; ++i) {
            if (m_data[i].high > maxP) maxP = m_data[i].high;
            if (m_data[i].low  < minP) minP = m_data[i].low;
        }
        if (maxP > minP) {
            const double margin = (maxP - minP) * 0.04;
            maxP += margin; minP -= margin;
            const double frac = double(mainPlotRect.bottom() - mouseY) / mainPlotRect.height();
            const double price = minP + frac * (maxP - minP);
            const QString txt = QString::number(price, 'f', 2);
            // 轴宽 56px 放不下带 padding 的文字，此处取实际文字宽度，
            // 保证读数完整显示；浮标覆盖在轴上会遮住同高度的刻度，
            // 这与专业行情软件一致（当前读数优先）。
            const int tw = qMax(fm.width(txt) + 10, priceAxisRect.width());
            const int th = fm.height() + 4;
            int ty = mouseY - th / 2;
            ty = qBound(priceAxisRect.top(), ty, priceAxisRect.bottom() - th);
            QRect tag(priceAxisRect.left(), ty, tw, th);
            drawTag(tag, txt, QColor(0xff, 0xe0, 0x8a));
        }
    }

    // ---- 副图轴浮标 ----
    if (inSub) {
        QString txt;
        bool valid = true;

        if (m_subIndicator == SubIndicatorType::Volume) {
            double maxVol = 0;
            for (int i = start; i < m_data.size() && i < start + vis; ++i)
                maxVol = qMax(maxVol, m_data[i].volume);
            if (maxVol <= 0) maxVol = 1;
            const double frac = double(subPlotRect.bottom() - mouseY) / subPlotRect.height();
            const double v = frac * maxVol;
            if (v >= 1e8)      txt = QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
            else if (v >= 1e4) txt = QString::number(v / 1e4, 'f', 1) + QStringLiteral("万");
            else               txt = QString::number(v, 'f', 0);
        } else if (m_subIndicator == SubIndicatorType::MoneyFlow && !m_flowNet.isEmpty()) {
            double mx = 0, mn = 0;
            for (int i = start; i < m_data.size() && i < start + vis && i < m_flowNet.size(); ++i) {
                mx = qMax(mx, m_flowNet[i]);
                mn = qMin(mn, m_flowNet[i]);
            }
            if (mx == 0 && mn == 0) { mx = 1e8; mn = -1e8; }
            if (mx == mn) { mx += 1e6; mn -= 1e6; }
            const double mrg = (mx - mn) * 0.08;
            mx += mrg; mn -= mrg;
            const double frac = double(subPlotRect.bottom() - mouseY) / subPlotRect.height();
            const double v = mn + frac * (mx - mn);
            const double a = qAbs(v);
            if (a >= 1e8)      txt = QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
            else if (a >= 1e4) txt = QString::number(v / 1e4, 'f', 0) + QStringLiteral("万");
            else               txt = QString::number(v, 'f', 0);
        } else if (m_subIndicator == SubIndicatorType::MACD) {
            double mx = -1e15, mn = 1e15;
            for (int i = start; i < m_data.size() && i < start + vis; ++i) {
                mx = qMax(mx, qMax(m_macdDIF[i], qMax(m_macdDEA[i], m_macdBar[i])));
                mn = qMin(mn, qMin(m_macdDIF[i], qMin(m_macdDEA[i], m_macdBar[i])));
            }
            if (mn > 0) mn = -0.01;
            if (mx < 0) mx = 0.01;
            if (mx <= mn) { mx = mn + 1; mn -= 1; }
            const double mrg = (mx - mn) * 0.05;
            mx += mrg; mn -= mrg;
            const double frac = double(subPlotRect.bottom() - mouseY) / subPlotRect.height();
            txt = QString::number(mn + frac * (mx - mn), 'f', 3);
        } else if (m_subIndicator == SubIndicatorType::KDJ) {
            double mx = 100, mn = 0;
            for (int i = start; i < m_data.size() && i < start + vis; ++i) {
                if (m_kdjK[i] == 0) continue;
                mx = qMax(mx, qMax(m_kdjK[i], qMax(m_kdjD[i], m_kdjJ[i])));
                mn = qMin(mn, qMin(m_kdjK[i], qMin(m_kdjD[i], m_kdjJ[i])));
            }
            if (mx <= mn) { mx = mn + 1; mn -= 1; }
            const double mrg = (mx - mn) * 0.05;
            mx += mrg; mn -= mrg;
            const double frac = double(subPlotRect.bottom() - mouseY) / subPlotRect.height();
            txt = QString::number(mn + frac * (mx - mn), 'f', 2);
        } else {
            valid = false;
        }

        if (valid && !txt.isEmpty()) {
            const int tw = qMax(fm.width(txt) + 10, subAxisRect.width());
            const int th = fm.height() + 4;
            int ty = mouseY - th / 2;
            ty = qBound(subAxisRect.top(), ty, subAxisRect.bottom() - th);
            QRect tag(subAxisRect.left(), ty, tw, th);
            drawTag(tag, txt, QColor(0xff, 0xe0, 0x8a));
        }
    }

    // ---- 时间轴日期浮标 ----
    {
        const QString dateTxt = m_data[idx].date;
        const int tw = fm.width(dateTxt) + 10;
        const int th = fm.height() + 4;
        const double cx = candleCenterX(cnt);
        int tx = int(cx) - tw / 2;
        tx = qBound(timeAxisRect.left(), tx, timeAxisRect.right() - tw);
        QRect tag(tx, timeAxisRect.top() + (timeAxisRect.height() - th) / 2, tw, th);
        drawTag(tag, dateTxt, QColor(0xe8, 0xee, 0xfc));
    }

    p.restore();
}

// =====================================================================
// 通用指示器线绘制
// =====================================================================
void KLineChart::drawIndicatorLine(QPainter &p, const QVector<double> &series,
                                   int start, int vis,
                                   const std::function<double(double)> &yPos,
                                   const QPen &pen)
{
    if (series.isEmpty()) return;
    p.setPen(pen);
    QPainterPath path;
    bool first = true;
    for (int cnt = 0; cnt < vis; ++cnt) {
        int i = start + cnt;
        if (i >= series.size()) break;
        double v = series[i];
        if (v == 0.0) continue;
        double cx = candleCenterX(cnt);
        double cy = yPos(v);
        if (first) { path.moveTo(cx, cy); first = false; }
        else path.lineTo(cx, cy);
    }
    if (!path.isEmpty())
        p.drawPath(path);
}

// =====================================================================
// 鼠标交互
// =====================================================================
void KLineChart::wheelEvent(QWheelEvent *ev)
{
    int delta = ev->angleDelta().y();
    if (delta > 0)
        m_candleW = qMin(K_MAX_W, m_candleW + 0.5);
    else
        m_candleW = qMax(K_MIN_W, m_candleW - 0.5);

    m_crosshair = false;
    m_hoverIdx = m_data.size() - 1;
    emit hoveredChanged(hoveredOrLatest());
    update();
}

void KLineChart::mousePressEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton) {
        if (m_splitHandleRect.contains(ev->pos())) {
            m_splitDragging = true;
            m_dragLastPos = ev->pos();
            setCursor(Qt::SplitVCursor);
            return;
        }
        m_dragging = true;
        m_dragLastPos = ev->pos();
        m_crosshair = false;
    }
}

void KLineChart::mouseMoveEvent(QMouseEvent *ev)
{
    // 分割手柄拖拽优先
    if (m_splitDragging) {
        int dy = ev->pos().y() - m_dragLastPos.y();
        if (dy != 0) {
            int totalH = height() - T_MARGIN - B_MARGIN - TIME_H;
            double newRatio = m_splitRatio + (double)dy / totalH;
            setSplitRatio(newRatio);
            m_dragLastPos = ev->pos();
        }
        return;
    }

    // 滚动平移
    if (m_dragging) {
        int dx = ev->pos().x() - m_dragLastPos.x();
        if (qAbs(dx) >= (int)m_candleW) {
            m_scrollOffset = qBound(0, m_scrollOffset + dx / (int)m_candleW,
                                     qMax(0, m_data.size() - 1));
            m_dragLastPos = ev->pos();
            m_crosshair = false;
            update();
        }
        return;
    }

    // 悬停检测手柄
    if (m_splitHandleRect.contains(ev->pos()) && !m_dragging)
        setCursor(Qt::SplitVCursor);
    else
        setCursor(Qt::ArrowCursor);

    // 十字光标
    bool inChart = mainPlotRect.contains(ev->pos()) ||
                   subPlotRect.contains(ev->pos());
    if (inChart) {
        int idx = idxAtX(ev->pos().x());
        if (idx >= 0 && idx < m_data.size()) {
            m_crosshair = true;
            m_hoverIdx = idx;
            m_mousePos = ev->pos();
            emit hoveredChanged(m_data[idx]);
        } else {
            m_crosshair = false;
        }
    } else {
        m_crosshair = false;
    }
    update();
}

void KLineChart::mouseReleaseEvent(QMouseEvent *)
{
    if (m_splitDragging) {
        m_splitDragging = false;
        setCursor(Qt::ArrowCursor);
    }
    m_dragging = false;
}

void KLineChart::leaveEvent(QEvent *)
{
    m_crosshair = false;
    m_hoverIdx = m_data.size() - 1;
    emit hoveredChanged(hoveredOrLatest());
    update();
}

void KLineChart::resizeEvent(QResizeEvent *)
{
    update();
}

// =====================================================================
// 刻度算法
// =====================================================================
QVector<double> KLineChart::niceTicks(double lo, double hi, int maxTicks)
{
    QVector<double> out;
    if (lo >= hi) return out;

    double range = hi - lo;
    double rough = range / maxTicks;

    double mag = qPow(10, qFloor(qLn(rough) / qLn(10)));
    double step = mag;
    for (double c : {1.0, 2.0, 5.0, 10.0}) {
        double s = mag * c;
        if (s < rough) continue;
        step = s;
        break;
    }

    double start = qFloor(lo / step) * step;
    for (double t = start; t <= hi + step * 0.5; t += step)
        out.append(t);
    return out;
}

// =====================================================================
// KLineInfoPanel — 右侧信息面板
// =====================================================================
KLineInfoPanel::KLineInfoPanel(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(190);
    setStyleSheet(QStringLiteral("background:#111a2e;color:%1;")
                  .arg(KLineChart::C_TEXT.name()));
}

void KLineInfoPanel::setData(const KLineData &kd, const KLineData *prevClose)
{
    m_data = kd;
    if (prevClose) {
        m_prev = *prevClose;   // 值拷贝，杜绝悬空指针
        m_hasPrev = true;
    } else {
        m_prev = KLineData();
        m_hasPrev = false;
    }
    m_hasData = true;
    update();
}

void KLineInfoPanel::clear()
{
    m_hasData = false;
    update();
}

void KLineInfoPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!m_hasData) return;

    p.setFont(QFont("Microsoft YaHei", 9));
    QFontMetrics fm(p.font());
    int lineH = fm.height() + 6;
    int textH = fm.height();
    int xL = 14, y = 14;

    // 用 QRect 绘制文字，y 始终代表行顶部（不是基线），避免分隔线与文字重叠
    auto drawRow = [&](const QString &label, const QString &value, const QColor &color) {
        p.setPen(KLineChart::C_TEXT);
        p.drawText(QRect(xL, y, 60, textH), Qt::AlignLeft | Qt::AlignVCenter, label);
        p.setPen(color);
        int vw = fm.width(value);
        p.drawText(QRect(width() - vw - 14, y, vw, textH), Qt::AlignRight | Qt::AlignVCenter, value);
        y += lineH;
    };

    // 标题
    p.setPen(QColor(0x4d, 0xa3, 0xff));
    QFont titleF = p.font(); titleF.setBold(true); p.setFont(titleF);
    QFontMetrics tfm(titleF);
    p.drawText(QRect(xL, y, 120, tfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("行情数据"));
    y += lineH;
    p.setFont(QFont("Microsoft YaHei", 9));

    QColor col = KLineInfoPanel::C_FLAT;
    double chg = m_data.chgPct;
    double prevC = m_hasPrev ? m_prev.close : 0;
    if (qAbs(prevC) > 1e-9) {
        chg = (m_data.close - prevC) / prevC * 100;
        col = (chg > 0) ? C_UP : ((chg < 0) ? C_DOWN : C_FLAT);
    } else if (qAbs(m_data.chgPct) > 1e-9) {
        col = (m_data.chgPct > 0) ? C_UP : ((m_data.chgPct < 0) ? C_DOWN : C_FLAT);
    }

    // 分隔线1 — 标题与价格数据之间
    y += 4;
    p.setPen(QColor(0x22, 0x33, 0x4d));
    p.drawLine(xL, y, width() - 14, y);
    y += 8;

    drawRow(QStringLiteral("日期"), m_data.date.left(10), KLineChart::C_TEXT);
    drawRow(QStringLiteral("收盘"),
            QString::number(m_data.close, 'f', 2), col);
    // 涨跌额：以昨收(前一根K线收盘)为基准；无昨收时显示 "--" 而非把收盘价当涨跌
    const double prevClose = (m_hasPrev && m_prev.close > 0) ? m_prev.close : 0.0;
    if (prevClose > 0) {
        const double chgAmt = m_data.close - prevClose;
        drawRow(QStringLiteral("涨跌"),
                QString("%1%2").arg(chgAmt > 0 ? "+" : "").arg(chgAmt, 0, 'f', 2), col);
    } else {
        drawRow(QStringLiteral("涨跌"), QStringLiteral("--"), col);
    }

    const bool hasPrevClose = (m_hasPrev && m_prev.close > 0);
    if (hasPrevClose) {
        drawRow(QStringLiteral("涨幅"),
                QString("%1%2%").arg(chg > 0 ? "+" : "").arg(chg, 0, 'f', 2), col);
    } else {
        drawRow(QStringLiteral("涨幅"), QStringLiteral("--"), KLineInfoPanel::C_FLAT);
    }

    // 分隔线2 — 价格与OHLC之间
    y += 4;
    p.setPen(QColor(0x22, 0x33, 0x4d));
    p.drawLine(xL, y, width() - 14, y);
    y += 8;

    // 统一标签色
    auto drawInfoRow = [&](const QString &text) {
        p.setPen(KLineChart::C_TEXT);
        p.drawText(QRect(xL, y, 120, textH), Qt::AlignLeft | Qt::AlignVCenter, text);
        y += lineH;
    };

    drawInfoRow(QStringLiteral("开盘：%1").arg(m_data.open, 0, 'f', 2));
    drawInfoRow(QStringLiteral("最高：%1").arg(m_data.high, 0, 'f', 2));
    drawInfoRow(QStringLiteral("最低：%1").arg(m_data.low, 0, 'f', 2));

    // 昨收：优先取前一根K线的收盘价
    const double pc = (m_hasPrev && m_prev.close > 0) ? m_prev.close : 0.0;
    drawInfoRow(QStringLiteral("昨收：%1")
                    .arg(pc > 0 ? QString::number(pc, 'f', 2) : QStringLiteral("--")));
    y += lineH;

    // 分隔线3 — OHLC与成交量之间
    y += 4;
    p.setPen(QColor(0x22, 0x33, 0x4d));
    p.drawLine(xL, y, width() - 14, y);
    y += 8;

    auto fmtVol = [](double v) -> QString {
        if (v >= 1e8) return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿手");
        if (v >= 1e4) return QString::number(v / 1e4, 'f', 0) + QStringLiteral("万手");
        return QString::number(v, 'f', 0);
    };
    p.setPen(KLineChart::C_TEXT);
    p.drawText(QRect(xL, y, 120, textH), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("成交量：%1").arg(fmtVol(m_data.volume)));
    y += lineH;

    auto fmtAmt = [](double v) -> QString {
        if (v >= 1e8) return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
        if (v >= 1e4) return QString::number(v / 1e4, 'f', 0) + QStringLiteral("万");
        return QString::number(v, 'f', 0);
    };
    // 成交额：新浪备选源不提供，此时明确标注"源未提供"而非易误解的 "--"
    p.drawText(QRect(xL, y, width() - 28, textH), Qt::AlignLeft | Qt::AlignVCenter,
               m_data.amount > 0
                   ? QStringLiteral("成交额：%1").arg(fmtAmt(m_data.amount))
                   : QStringLiteral("成交额：（该数据源未提供）"));
    y += lineH;

    // 换手率：同样可能缺失；缺失时不占行，避免出现空行
    if (m_data.turnover > 0) {
        p.drawText(QRect(xL, y, 120, textH), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("换手率：%1%").arg(m_data.turnover, 0, 'f', 2));
        y += lineH;
    }
    // 振幅：缺失时用 (最高-最低)/昨收 推算，保证该行始终有意义
    {
        double amp = m_data.amplitude;
        if (amp <= 0 && m_hasPrev && m_prev.close > 0 && m_data.high > m_data.low)
            amp = (m_data.high - m_data.low) / m_prev.close * 100.0;
        if (amp > 0) {
            p.drawText(QRect(xL, y, 120, textH), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("振  幅：%1%%2").arg(amp, 0, 'f', 2)
                           .arg(m_data.amplitude > 0 ? QString() : QStringLiteral("*")));
            y += lineH;
        }
    }

    // ---- 补充区：把面板下半部空白利用起来 ----
    // 这些信息在看盘时比"再多一条原始行情"更有价值：
    // K线形态、均线多空排列、量能状态。
    auto drawInfoRowC = [&](const QString &text, const QColor &c) {
        p.setPen(c);
        p.drawText(QRect(xL, y, width() - 28, textH), Qt::AlignLeft | Qt::AlignVCenter, text);
        y += lineH;
    };

    // 分隔线4
    y += 4;
    p.setPen(QColor(0x22, 0x33, 0x4d));
    p.drawLine(xL, y, width() - 14, y);
    y += 8;

    // K 线形态：根据开盘/收盘/最高/最低判断
    {
        QString shape;
        QColor shapeColor = KLineChart::C_TEXT;
        const double body = qAbs(m_data.close - m_data.open);
        const double range = m_data.high - m_data.low;
        const double upperShadow = m_data.high - qMax(m_data.open, m_data.close);
        const double lowerShadow = qMin(m_data.open, m_data.close) - m_data.low;

        if (range > 1e-9 && body / range < 0.1) {
            shape = QStringLiteral("十字星");
        } else if (range > 1e-9 && upperShadow > body * 2 && lowerShadow < body) {
            shape = (m_data.close < m_data.open) ? QStringLiteral("上影长·上方压力")
                                                 : QStringLiteral("上影长·冲高回落");
        } else if (range > 1e-9 && lowerShadow > body * 2 && upperShadow < body) {
            shape = (m_data.close > m_data.open) ? QStringLiteral("下影长·下方支撑")
                                                 : QStringLiteral("下影长·探底回升");
        } else if (m_data.close > m_data.open) {
            shape = (body / qMax(range, 1e-9) > 0.7) ? QStringLiteral("大阳线")
                                                     : QStringLiteral("阳线");
            shapeColor = C_UP;
        } else if (m_data.close < m_data.open) {
            shape = (body / qMax(range, 1e-9) > 0.7) ? QStringLiteral("大阴线")
                                                     : QStringLiteral("阴线");
            shapeColor = C_DOWN;
        } else {
            shape = QStringLiteral("平盘");
        }
        drawInfoRowC(QStringLiteral("形态：%1").arg(shape), shapeColor);
    }

    // 量能：与 5 日均量比较
    {
        // 振幅可能缺失（新浪源不提供），此时用 (最高-最低)/昨收 自行计算
        double amp = m_data.amplitude;
        if (amp <= 0 && m_hasPrev && m_prev.close > 0 && m_data.high > m_data.low)
            amp = (m_data.high - m_data.low) / m_prev.close * 100.0;

        QString volTxt;
        QColor volColor = KLineChart::C_TEXT;
        if (amp > 0) {
            if (amp >= 7.0)      { volTxt = QStringLiteral("剧烈波动"); volColor = C_UP; }
            else if (amp >= 4.0) { volTxt = QStringLiteral("波动较大"); }
            else if (amp <= 1.5) { volTxt = QStringLiteral("窄幅整理"); }
            else                 { volTxt = QStringLiteral("波动正常"); }
            drawInfoRowC(QStringLiteral("振幅评价：%1").arg(volTxt), volColor);
        }
    }
}
