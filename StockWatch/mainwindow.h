#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QDate>
#include <QPointer>
#include <QSystemTrayIcon>

#include "riskscanner.h"
#include "holdingcalc.h"
#include "sectordata.h"

class QTableWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QLineEdit;
class QComboBox;
class QDialog;
class QMenu;
class ReportGenerator;
class SectorDialog;

// ---------- 持仓记录 ----------
// 注：内部计算统一使用 holdingcalc.h 的 TradeRecord，HoldingRecord 保留为
// 持久化格式的兼容别名，避免历史 config.ini 失效。
using HoldingRecord = TradeRecord;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void closeEvent(QCloseEvent *ev) override;
    void showEvent(QShowEvent *ev) override;

private slots:
    void addStock();
    void refresh();
    void onReplyFinished(QNetworkReply *reply);
    void setAlert();
    void removeStock();
    void onIntervalChanged(int index);
    void onTableDoubleClicked(int row, int column);
    void generateReports();
    void onRiskResult(const QString &code, const RiskResult &result);

private:
    void buildUi();
    void fetchQuotes();
    void fetchTencentQuotes();
    void parseTencentReply(const QByteArray &data);
    void renderTable();
    void updateStatusBar();
    void checkAlerts();
    void checkDropAlert();
    double limitPct(const QString &code, const QString &name) const;
    void saveConfig();
    void loadConfig();
    void updateMarketStatus();
    QString secId(const QString &code) const;
    void scanRisks();
    void updateRiskLabel();
    void clearRiskWarnings();

    // ---- 板块联动与相对强度 ----
    void openSectorAnalysis(const QString &code, const QString &name);
    void fetchSectors(const QString &code);
    void onSectorReply(QNetworkReply *reply);
    void updateSectorDialogs(const QString &code);

    // ---- 持仓 ----
    struct HoldingWidgets {
        QPointer<QDialog> dialog;
        QLabel *netLabel      = nullptr;
        QLabel *avgLabel      = nullptr;
        QLabel *buyCostLabel  = nullptr;
        QLabel *sellLabel     = nullptr;
        QLabel *curPriceLabel = nullptr;
        QLabel *pnlLabel      = nullptr;
        double  curPrice      = 0.0;   // 最近一次刷新时的行情价
        QTableWidget *table   = nullptr;
    };
    int  netShares(const QString &code) const;
    // 统一入口：按移动加权平均法计算持仓汇总（含费用）
    HoldingSummary holdingSummary(const QString &code, double currentPrice) const;
    HoldingSummary holdingSummaryNow(const QString &code) const;  // 用当前行情价
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
    QLabel              *m_riskLabel;        // 造假风险警告栏(红字)
    QWidget             *m_riskBar;          // 警告栏容器(含滚动区+清理按钮)
    QNetworkAccessManager *m_net;
    QTimer              *m_timer;
    QTimer              *m_countdownTimer;
    int                  m_countdownValue = 10;
    int                  m_currentIntervalSecs = 10;
    int                  m_retryCount = 0;       // 行情请求失败重试计数
    bool                 m_usingTencent = false; // 当前是否切到腾讯源
    QDate                m_lastClosedRefreshDate; // 休市日已补刷过的日期(当日不再轮询)
    int                  m_idleSkipCount = 0;     // 非交易时段跳过的刷新次数
    bool                 m_firstShown = false;    // 是否已完成首次刷新
    QTimer              *m_reportTimer;
    QDate                m_lastReportDate;
    ReportGenerator     *m_reporter;
    RiskScanner         *m_riskScanner;
    QMap<QString, RiskResult> m_riskResults;  // 已扫描风险结果缓存(供警告栏持续显示)

    QStringList m_stocks;
    QMap<QString, QMap<QString, QString>> m_quotes;
    QMap<QString, QMap<QString, double>> m_alerts;
    QMap<QString, qint64> m_triggered;
    QMap<QString, int> m_rowOfCode;

    // 持仓
    QMap<QString, QVector<HoldingRecord>> m_holdings;
    QMap<QString, HoldingWidgets> m_holdingWidgets;
    FeeConfig m_feeConfig;                 // 交易费用模型(佣金/印花税/过户费)

    // 系统托盘
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu           *m_trayMenu  = nullptr;

    // 板块联动缓存：code -> 联动结果（含所属板块与相对强度）
    QMap<QString, SectorLink>   m_sectorLinks;
    QMap<QString, SectorDialog *> m_sectorDialogs;   // 已打开的板块分析窗口
};

#endif // MAINWINDOW_H
