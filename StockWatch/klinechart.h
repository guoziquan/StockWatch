#ifndef KLINECHART_H
#define KLINECHART_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QColor>

// ---------- 单根K线 ----------
struct KLineData
{
    QString date;    // YYYY-MM-DD 或 YYYY-MM-DD HH:mm
    double  open;
    double  close;
    double  high;
    double  low;
    double  volume;  // 成交量(手)
};

// ---------- 自定义蜡烛图控件 ----------
class KLineChart : public QWidget
{
    Q_OBJECT
public:
    explicit KLineChart(QWidget *parent = nullptr);

    void setData(const QVector<KLineData> &data);
    void clear();

    static const QColor COLOR_UP;      // 阳线红
    static const QColor COLOR_DOWN;    // 阴线绿
    static const QColor COLOR_BG;
    static const QColor COLOR_GRID;
    static const QColor COLOR_TEXT;

protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    int  candleWidth() const;          // 每根K线占用像素宽
    int  visibleCount() const;         // 当前视图能显示的K线数量
    int  idxAtPos(int x) const;        // 像素x对应的数据下标
    void drawCandles(QPainter &p, const QRect &area);
    void drawVolumes(QPainter &p, const QRect &area);
    void drawAxis(QPainter &p, const QRect &ka, const QRect &va);
    void drawCrosshair(QPainter &p);

    QVector<KLineData> m_data;

    // 可视范围
    int  m_scrollOffset = 0;           // 右侧偏移（0=最新数据在右端）
    int  m_candleW       = 8;          // 缩放系数
    int  m_gap           = 1;          // 蜡烛间距

    // 拖拽
    bool m_dragging = false;
    QPoint m_lastPos;

    // 十字光标
    bool m_crosshair = false;
    int  m_crossIdx  = -1;
    QPoint m_crossPos;

    // 布局比例
    double m_klineRatio = 0.72;        // 主图占72%高

    // 边距
    static const int MARGIN_L = 10;
    static const int MARGIN_R = 76;
    static const int MARGIN_T = 8;
    static const int MARGIN_B = 18;
    static const int GAP_MID  = 4;     // 主图与量图间距
};

#endif // KLINECHART_H
