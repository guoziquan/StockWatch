#ifndef TIMESHARECHART_H
#define TIMESHARECHART_H

#include <QWidget>
#include <QVector>
#include <QColor>
#include "timesharedata.h"
#include "pricelimit.h"

// ============================================================================
// 分时图控件（QPainter 自绘）
//
// 与 KLineChart 的区别：
//   * 横轴固定 240 格（全天交易分钟），数据不足时右侧留白
//   * 主图：价格线 + 均价线 + 昨收基准线 + 涨跌停虚线 + 填充
//   * 副图：每分钟成交量柱
//   * 左轴：价格（含涨跌幅%），右轴：涨跌幅百分比
//   * 十字光标带浮标
// ============================================================================
class TimeShareChart : public QWidget
{
    Q_OBJECT
public:
    explicit TimeShareChart(QWidget *parent = nullptr);

    void setData(const TimeShareData &d);
    void clear();
    bool hasData() const { return m_data.valid; }
    const TimeShareData &data() const { return m_data; }

public:
    static const QColor C_UP;
    static const QColor C_DOWN;
    static const QColor C_FLAT;
    static const QColor C_BG;
    static const QColor C_GRID;
    static const QColor C_TEXT;
    static const QColor C_CROSS;
    static const QColor C_AVG;    // 均价线（黄色）
    static const QColor C_PRICE;  // 价格线（白色）

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    // 布局
    void calcLayout();
    // 绘制
    void drawBackground(QPainter &p);
    void drawGrid(QPainter &p);
    void drawPriceArea(QPainter &p);
    void drawVolumeArea(QPainter &p);
    void drawAxes(QPainter &p);
    void drawTimeAxis(QPainter &p);
    void drawCrosshair(QPainter &p);

    // 坐标换算
    double slotToX(double slot) const;
    double priceToY(double price) const;

    TimeShareData m_data;
    PriceLimit    m_limit;

    // 缓存数据点对应槽位
    QVector<int> m_slots;

    // 布局矩形
    QRect m_plotRect;      // 主图（价格）
    QRect m_volRect;       // 副图（成交量）
    QRect m_leftAxis;      // 左轴
    QRect m_rightAxis;     // 右轴
    QRect m_timeAxis;      // 时间轴

    // 价格范围
    double m_minP = 0;
    double m_maxP = 0;

    // 十字光标
    bool m_hover = false;
    QPoint m_mousePos;
    int m_hoverSlot = -1;

    // 常量
    static const int L_MARGIN;
    static const int AXIS_W;
    static const int TIME_H;
    static const int VOL_RATIO_NUM;   // 副图高度占比分子
    static const int VOL_RATIO_DEN;
};

#endif // TIMESHARECHART_H
