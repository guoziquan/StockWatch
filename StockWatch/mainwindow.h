#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QDate>
#include <QPointer>
#include <QSystemTrayIcon>

class QTableWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QLineEdit;
class QComboBox;
class QDialog;
class QMenu;
class ReportGenerator;

// ---------- 持仓记录 ----------
struct HoldingRecord {
    QString code;      // 股票代码
    bool    isBuy;     // true=买入, false=卖出
    int     shares;    // 股数（始终 >0）
    double  price;     // 买入价或卖出价
    QDate   date;      // 交易日
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void closeEvent(QCloseEvent *ev) override;

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

    // ---- 持仓 ----
    struct HoldingWidgets {
        QPointer<QDialog> dialog;
        QLabel *netLabel      = nullptr;
        QLabel *avgLabel      = nullptr;
        QLabel *buyCostLabel  = nullptr;
        QLabel *sellLabel     = nullptr;
        QLabel *curPriceLabel = nullptr;
        QLabel *pnlLabel      = nullptr;
        QTableWidget *table   = nullptr;
    };
    int  netShares(const QString &code) const;
    double avgCost(const QString &code) const;
    double totalBuyCost(const QString &code) const;
    double totalSellIncome(const QString &code) const;
    double holdingPnl(const QString &code, double currentPrice) const;
    void buyStock(const QString &code, const QString &name);
    void sellStock(const QString &code, const QString &name);
    void openHoldingDetail(const QString &code);
    void refreshHoldingDialog(const QString &code);
    void refreshHoldingDialogs();

    QTableWidget        *m_table;
    QLineEdit           *m_codeEdit;
    QComboBox           *m_intervalCombo;
    QLabel              *m_statusLabel;
    QLabel              *m_mktLabel;
    QLabel              *m_countdownLabel;
    QLabel              *m_idxSh, *m_idxSz, *m_idxCy;
    QNetworkAccessManager *m_net;
    QTimer              *m_timer;
    QTimer              *m_countdownTimer;
    int                  m_countdownValue = 10;
    int                  m_currentIntervalSecs = 10;
    QTimer              *m_reportTimer;
    QDate                m_lastReportDate;
    ReportGenerator     *m_reporter;

    QStringList m_stocks;
    QMap<QString, QMap<QString, QString>> m_quotes;
    QMap<QString, QMap<QString, double>> m_alerts;
    QMap<QString, qint64> m_triggered;
    QMap<QString, int> m_rowOfCode;

    // 持仓
    QMap<QString, QVector<HoldingRecord>> m_holdings;
    QMap<QString, HoldingWidgets> m_holdingWidgets;

    // 系统托盘
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu           *m_trayMenu  = nullptr;
};

#endif // MAINWINDOW_H
