#include "autoclosemsg.h"

AutoCloseMessageBox::AutoCloseMessageBox(QWidget *parent,
                                         Icon icon,
                                         const QString &title,
                                         const QString &text,
                                         int timeoutSeconds)
    : QMessageBox(parent)
    , m_baseTitle(title)
{
    setIcon(icon);
    setWindowTitle(title);
    setText(text);
    setAttribute(Qt::WA_DeleteOnClose);

    if (timeoutSeconds > 0) {
        m_remaining = timeoutSeconds;
        m_timer = new QTimer(this);
        m_timer->setInterval(1000);
        connect(m_timer, &QTimer::timeout, this, &AutoCloseMessageBox::tick);
        m_timer->start();
        // 初始显示倒计时
        setWindowTitle(QStringLiteral("%1 (%2s)").arg(m_baseTitle).arg(m_remaining));
        open();  // 窗口级模态：阻止父窗口但不阻塞事件循环
    } else {
        exec();  // 无倒计时 → 同步阻塞，行为与普通 QMessageBox 一致
    }
}

void AutoCloseMessageBox::tick()
{
    m_remaining--;
    if (m_remaining <= 0) {
        m_timer->stop();
        close();
        return;
    }
    setWindowTitle(QStringLiteral("%1 (%2s)").arg(m_baseTitle).arg(m_remaining));
}

void AutoCloseMessageBox::info(QWidget *parent, const QString &title,
                               const QString &text, int timeoutSec)
{
    auto *box = new AutoCloseMessageBox(parent, Information, title, text, timeoutSec);
    Q_UNUSED(box);  // WA_DeleteOnClose handles cleanup
}

void AutoCloseMessageBox::warn(QWidget *parent, const QString &title,
                               const QString &text, int timeoutSec)
{
    auto *box = new AutoCloseMessageBox(parent, Warning, title, text, timeoutSec);
    Q_UNUSED(box);
}
