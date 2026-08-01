#ifndef STOCKCHARTWINDOW_H
#define STOCKCHARTWINDOW_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QNetworkAccessManager>
#include "klinechart.h"

class StockChartWindow : public QWidget
{
    Q_OBJECT
public:
    explicit StockChartWindow(const QString &code, const QString &name,
                              QNetworkAccessManager *sharedNet,
                              QWidget *parent = nullptr);

private slots:
    void fetchKLine();
    void onReplyFinished(QNetworkReply *reply);
    void onPeriodChanged(int index);
    void onMainIndicatorChanged(int index);
    void onSubIndicatorChanged(int index);

private:
    void buildUi();
    QString secId() const;

    QString m_code;
    QString m_name;
    int m_klt = 101;

    KLineChart            *m_chart;
    KLineInfoPanel        *m_infoPanel;
    QComboBox             *m_periodCombo;
    QComboBox             *m_mainIndCombo;
    QComboBox             *m_subIndCombo;
    QLabel                *m_titleLabel;
    QLabel                *m_statusLabel;
    QNetworkAccessManager *m_net;
    QVector<KLineData>     m_cacheData;
};

#endif // STOCKCHARTWINDOW_H
