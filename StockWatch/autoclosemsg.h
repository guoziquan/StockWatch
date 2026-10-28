#ifndef AUTOCLOSEMSG_H
#define AUTOCLOSEMSG_H

#include <QMessageBox>
#include <QTimer>

// 自动关闭的 QMessageBox 子类
// - timeoutSeconds = 0: 普通消息框，exec() 阻塞，不自动关闭
// - timeoutSeconds > 0: open() 窗口级模态，标题显示倒计时，到时自动关闭
class AutoCloseMessageBox : public QMessageBox
{
    Q_OBJECT
public:
    explicit AutoCloseMessageBox(QWidget *parent,
                                 Icon icon,
                                 const QString &title,
                                 const QString &text,
                                 int timeoutSeconds = 0);

    // 静态便捷方法
    static void info(QWidget *parent, const QString &title,
                     const QString &text, int timeoutSec = 5);
    static void warn(QWidget *parent, const QString &title,
                     const QString &text, int timeoutSec = 5);

private slots:
    void tick();

private:
    QTimer  *m_timer        = nullptr;
    int      m_remaining    = 0;
    QString  m_baseTitle;
};

#endif // AUTOCLOSEMSG_H
