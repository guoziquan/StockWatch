#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QDate>

class QTableWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QLineEdit;
class QComboBox;
class ReportGenerator;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void addStock();
    void refresh();
    void onReplyFinished(QNetworkReply *reply);
    void setAlert();
    void removeStock();
    void onIntervalChanged(int index);
    void onTableDoubleClicked(int row, int column);
    void generateReports();

private:
    void buildUi();
    void fetchQuotes();
    void renderTable();
    void updateStatusBar();
    void checkAlerts();
    void saveConfig();
    void loadConfig();
    void updateMarketStatus();
    QString secId(const QString &code) const;

    QTableWidget        *m_table;
    QLineEdit           *m_codeEdit;
    QComboBox           *m_intervalCombo;
    QLabel              *m_statusLabel;
    QLabel              *m_mktLabel;
    QLabel              *m_countdownLabel;
    QLabel              *m_idxSh, *m_idxSz, *m_idxCy;  // 上证/深证/创业板
    QNetworkAccessManager *m_net;
    QTimer              *m_timer;
    QTimer              *m_countdownTimer;
    int                  m_countdownValue = 10;
    int                  m_currentIntervalSecs = 10;
    QTimer              *m_reportTimer;
    QDate                m_lastReportDate;
    ReportGenerator     *m_reporter;

    QStringList m_stocks;                          // 自选股代码列表
    QMap<QString, QMap<QString, QString>> m_quotes; // code -> {name,price,chgPct,...}
    QMap<QString, QMap<QString, double>> m_alerts;  // code -> {up,down}
    QMap<QString, qint64> m_triggered;              // 防重复提醒
    QMap<QString, int> m_rowOfCode;                 // 代码 -> 表格行号
};

#endif // MAINWINDOW_H
