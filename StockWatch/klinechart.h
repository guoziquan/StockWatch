#ifndef KLINECHART_H
#define KLINECHART_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QColor>
#include <QDate>
#include <QFont>
#include <functional>

// ==================== 主图指示器类型 ====================
enum class MainIndicatorType { None, MA, BOLL };

// ==================== 副图指示器类型 ====================
enum class SubIndicatorType  { Volume, MACD, KDJ, MoneyFlow };

// ==================== 资金流条目(独立于 indicator.h, 避免循环依赖) ====================
struct FlowPoint
{
    QString date;           // yyyy-MM-dd
    double  mainNet   = 0;  // 主力净流入(元) = 超大单 + 大单
    double  mainRatio = 0;  // 主力净占比(%)
};

// ==================== 单根K线数据 ====================
struct KLineData
{
    QString date;       // YYYY-MM-DD 或 YYYY-MM-DD HH:mm
    double  open   = 0;
    double  close  = 0;
    double  high   = 0;
    double  low    = 0;
    double  volume = 0;     // 成交量(手)
    double  amount = 0;     // 成交额(元)
    double  turnover = 0;   // 换手率(%)
    double  amplitude = 0;  // 振幅(%)
    double  chgPct   = 0;   // 涨跌幅(%)

    bool valid() const { return !date.isEmpty(); }
};

// ==================== 右侧信息面板 ====================
class KLineInfoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit KLineInfoPanel(QWidget *parent = nullptr);
    void setData(const KLineData &kd, const KLineData *prevClose = nullptr);
    void clear();

public:
    static const QColor C_UP;
    static const QColor C_DOWN;
    static const QColor C_FLAT;

private:
    void paintEvent(QPaintEvent *) override;
    KLineData m_data;
    // 用值而非指针保存前一根K线：调用方传入的往往是临时对象指针
    // （如 &vec[vec.size()-2]），一旦外部容器销毁/重排就会悬空。
    KLineData m_prev;
    bool m_hasPrev = false;
    bool m_hasData = false;
};

// ==================== K线图表主控件 ====================
class KLineChart : public QWidget
{
    Q_OBJECT
public:
    explicit KLineChart(QWidget *parent = nullptr);

    void setData(const QVector<KLineData> &data);
    // 设置主力资金流数据(按日期对齐到K线; 缺失日期按 0 处理)
    void setMoneyFlow(const QVector<FlowPoint> &flows);
    void clear();
    const KLineData &hoveredOrLatest() const;
    bool hasData() const { return !m_data.isEmpty(); }
    bool hasMoneyFlow() const { return !m_flowNet.isEmpty(); }

    // 指示器切换
    void setMainIndicator(MainIndicatorType ind);
    void setSubIndicator(SubIndicatorType ind);
    MainIndicatorType mainIndicator() const { return m_mainIndicator; }
    SubIndicatorType  subIndicator()  const { return m_subIndicator; }

    // 主副图比例
    void   setSplitRatio(double ratio);
    double splitRatio() const { return m_splitRatio; }

signals:
    void hoveredChanged(const KLineData &kd);
    void splitRatioChanged(double ratio);

public: // 颜色常量
    static const QColor C_UP;
    static const QColor C_DOWN;
    static const QColor C_BG;
    static const QColor C_GRID;
    static const QColor C_TEXT;
    static const QColor C_CROSS;

protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    // ====== 布局常量 ======
    static const int L_MARGIN   = 10;   // 左留白
    static const int R_PADDING  = 12;   // 轴标签右侧到分隔线间距
    static const int T_MARGIN   = 8;    // 顶部留白（给均线图例）
    static const int B_MARGIN   = 4;    // 底部留白
    static const int AXIS_W     = 56;   // 价格/量坐标轴宽度
    static const int TIME_H     = 26;   // 时间轴高度
    static const int SPLIT_HANDLE_H = 6;// 分割手柄高度
    static const int MIN_MAIN_H = 220;  // 主图最低高度
    static const int MIN_SUB_H  = 120;  // 副图最低高度

    static constexpr double K_MIN_W = 3.0;
    static constexpr double K_MAX_W = 14.0;

    // ====== 布局矩形（每次paint前计算） ======
    QRect mainPlotRect;       // K线绘制区
    QRect priceAxisRect;      // 价格轴
    QRect subPlotRect;        // 副图绘制区
    QRect subAxisRect;        // 成交量/指标轴
    QRect timeAxisRect;       // 时间轴
    QRect m_splitHandleRect;  // 分割手柄热区

    // ====== 数据 ======
    QVector<KLineData> m_data;
    int    m_scrollOffset = 0;       // 右侧偏移（0=最新在右端）
    double m_candleW      = 7.0;     // 每根K线像素宽（含间距）

    // ====== 指示器状态 ======
    MainIndicatorType m_mainIndicator = MainIndicatorType::MA;
    SubIndicatorType  m_subIndicator  = SubIndicatorType::Volume;
    double m_splitRatio = 0.68;       // 主图占图表总高比例

    // ====== 缓存指示器序列 ======
    QVector<double> m_ma5, m_ma10, m_ma20, m_ma60;
    QVector<double> m_bollUp, m_bollMid, m_bollDown;
    QVector<double> m_macdDIF, m_macdDEA, m_macdBar;
    QVector<double> m_kdjK, m_kdjD, m_kdjJ;

    // ====== 资金流(与 m_data 等长,按日期对齐; 无数据处为0) ======
    QVector<double> m_flowNet;     // 主力净流入(元)
    QVector<double> m_flowRatio;   // 主力净占比(%)

    // ====== 成交量均线(5/10/20 日) ======
    QVector<double> m_volMa5, m_volMa10, m_volMa20;

    // ====== 十字光标 ======
    bool m_crosshair  = false;
    int  m_hoverIdx   = -1;
    QPoint m_mousePos;

    // ====== 拖拽 ======
    bool m_dragging = false;
    QPoint m_dragLastPos;
    bool m_splitDragging = false;

public:
    int  visibleCount() const;
    int  visibleStart() const;
private:
    int  idxAtX(int x) const;
    double candleCenterX(int visIdx) const;

    // ====== 布局计算 ======
    void calcLayout();

    // ====== 序列计算 ======
    void computeIndicatorSeries();

    // ====== 绘制总调度 ======
    void drawChart(QPainter &p);

    // ====== 各区域绘制 ======
    void drawMainGrid(QPainter &p);
    void drawCandles(QPainter &p);
    void drawMovingAverages(QPainter &p);
    void drawBollinger(QPainter &p);
    void drawVolumeBars(QPainter &p);
    void drawMACD(QPainter &p);
    void drawKDJ(QPainter &p);
    void drawMoneyFlow(QPainter &p);
    void drawPriceAxis(QPainter &p);
    void drawSubAxis(QPainter &p);
    void drawFlowAxisContent(QPainter &p);
    void drawVolumeAxisContent(QPainter &p);
    void drawIndicatorAxisContent(QPainter &p);
    void drawTimeAxis(QPainter &p);
    void drawCrosshair(QPainter &p);
    void drawCrosshairTags(QPainter &p);
    void drawSepLine(QPainter &p);
    void drawPanelBorder(QPainter &p);

    // ====== 辅助 ======
    void drawIndicatorLine(QPainter &p, const QVector<double> &series,
                           int start, int vis,
                           const std::function<double(double)> &yPos,
                           const QPen &pen);

    // ====== 刻度计算 ======
    static QVector<double> niceTicks(double lo, double hi, int maxTicks);
};

#endif // KLINECHART_H
