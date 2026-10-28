#ifndef STOCKCHARTWINDOW_H
#define STOCKCHARTWINDOW_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QStackedWidget>
#include "klinechart.h"
#include "timesharechart.h"

class StockChartWindow : public QWidget
{
    Q_OBJECT
public:
    explicit StockChartWindow(const QString &code, const QString &name,
                              QNetworkAccessManager *sharedNet,
                              QWidget *parent = nullptr);

private slots:
    void fetchKLine();
    void fetchSinaKLine();
    void fetchMoneyFlow();
    void onFlowReplyFinished(QNetworkReply *reply);
    void fetchTimeShare();
    void switchView(bool timeShare);
    void onReplyFinished(QNetworkReply *reply);
    void onSinaReplyFinished(QNetworkReply *reply);
    void onPeriodChanged(int index);
    void onMainIndicatorChanged(int index);
    void onSubIndicatorChanged(int index);

private:
    void buildUi();
    QString secId() const;

protected:
    void closeEvent(QCloseEvent *ev) override;

    QString m_code;
    QString m_name;
    int m_klt = 101;

    KLineChart            *m_chart;
    TimeShareChart        *m_tsChart = nullptr;   // 分时图
    QStackedWidget        *m_viewStack = nullptr; // K线/分时 切换容器
    bool                   m_isTimeShare = false; // 当前是否为分时视图
    QLabel                *m_flowTitle = nullptr;
    KLineInfoPanel        *m_infoPanel;
    QComboBox             *m_periodCombo;
    QComboBox             *m_mainIndCombo;
    QComboBox             *m_subIndCombo;
    QLabel                *m_titleLabel;
    QLabel                *m_statusLabel;
    QNetworkAccessManager *m_net;
    QVector<KLineData>     m_cacheData;
    bool                   m_triedSina = false; // 已尝试过新浪 fallback
    bool                   m_flowLoading = false; // 资金流请求在途
    QVector<FlowPoint>     m_flows;              // 已获取的资金流(按日期)
};

#endif // STOCKCHARTWINDOW_H
