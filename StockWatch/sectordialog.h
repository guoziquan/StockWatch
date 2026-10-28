#ifndef SECTORDIALOG_H
#define SECTORDIALOG_H

#include <QDialog>
#include <QString>
#include "sectordata.h"

class QLabel;
class QTableWidget;

// ============================================================================
// 板块联动分析窗口
//
// 独立成一个类而不是塞在 MainWindow 里：
//   * MainWindow 已经 1600+ 行，继续堆积会难以维护
//   * 独立类可以脱离主窗口单独渲染（便于自动化验证视觉）
// ============================================================================
class SectorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SectorDialog(const QString &code, const QString &name,
                          QWidget *parent = nullptr);

    // 更新数据（数据异步到达后调用）
    void setLink(const SectorLink &link);

    // 显示"加载中/失败"提示
    void setPending(const QString &hint);

    QString stockCode() const { return m_code; }

private:
    void buildUi();

    QString       m_code;
    QString       m_name;
    QLabel       *m_titleLabel   = nullptr;
    QLabel       *m_verdictLabel = nullptr;
    QLabel       *m_relLabel     = nullptr;
    QLabel       *m_hintLabel    = nullptr;
    QTableWidget *m_table        = nullptr;
};

#endif // SECTORDIALOG_H
