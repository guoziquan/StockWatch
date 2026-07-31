#ifndef KLINEDIALOG_H
#define KLINEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QNetworkAccessManager>
#include "klinechart.h"

class KLineDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KLineDialog(const QString &code, const QString &name,
                         QWidget *parent = nullptr);

private slots:
    void fetchKLine();
    void onReplyFinished(QNetworkReply *reply);

private:
    void buildUi();
    QString secId() const;

    QString m_code;
    QString m_name;
    int     m_klt = 101;  // 默认日K

    KLineChart            *m_chart;
    QComboBox             *m_periodCombo;
    QLabel                *m_titleLabel;
    QLabel                *m_statusLabel;
    QNetworkAccessManager *m_net;
};

#endif // KLINEDIALOG_H
