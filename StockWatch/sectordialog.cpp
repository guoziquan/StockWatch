#include "sectordialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QFont>

// 与主窗口一致的配色
static const QColor C_UP  (0xff, 0x4d, 0x5e);
static const QColor C_DOWN(0x2e, 0xe6, 0xa8);
static const QColor C_FLAT(0x8f, 0xa3, 0xc0);

SectorDialog::SectorDialog(const QString &code, const QString &name, QWidget *parent)
    : QDialog(parent, Qt::Window)
    , m_code(code)
    , m_name(name)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("%1 (%2) — 板块联动").arg(name, code));
    resize(680, 520);
    setMinimumSize(560, 400);
    setStyleSheet(QStringLiteral("background:#0b1220;color:#e8eefc;"));
    buildUi();
}

void SectorDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 10);
    root->setSpacing(8);

    // ---- 顶部结论区 ----
    auto *head = new QFrame(this);
    head->setStyleSheet(
        "QFrame{background:#0f1829;border:1px solid #22334d;border-radius:6px;}");
    auto *hl = new QVBoxLayout(head);
    hl->setContentsMargins(14, 10, 14, 10);
    hl->setSpacing(6);

    m_titleLabel = new QLabel(head);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(
        "color:#c8d6e5;font-size:12px;border:none;background:transparent;");
    hl->addWidget(m_titleLabel);

    m_verdictLabel = new QLabel(head);
    m_verdictLabel->setWordWrap(true);
    m_verdictLabel->setStyleSheet(
        "font-size:13px;font-weight:bold;border:none;background:transparent;");
    hl->addWidget(m_verdictLabel);

    m_relLabel = new QLabel(head);
    m_relLabel->setStyleSheet(
        "font-size:12px;border:none;background:transparent;");
    hl->addWidget(m_relLabel);

    root->addWidget(head);

    // ---- 板块列表 ----
    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("类型"),
                                        QStringLiteral("板块"),
                                        QStringLiteral("涨幅")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 62);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 92);
    m_table->setStyleSheet(
        "QTableWidget{background:#131f33;border:1px solid #22334d;border-radius:4px;"
        "color:#e8eefc;font-size:12px;}"
        "QTableWidget::item{padding:3px 6px;border-bottom:1px solid #1a2740;}"
        // 表头：深底 + 浅字。原先沿用主窗口的"深底+深字"组合，
        // 在本窗口的深色表头上几乎不可读，这里改为高对比配色。
        "QHeaderView::section{background:#1a2740;color:#a8c0e0;border:none;"
        "border-bottom:1px solid #22334d;padding:6px 4px;"
        "font-size:11px;font-weight:bold;}"
        "QHeaderView::section:hover{background:#1a2740;color:#a8c0e0;}");
    // 表头 item 前景色不要再覆盖成深色（否则又变暗）
    for (int c = 0; c < 3; ++c) {
        auto *hi = m_table->horizontalHeaderItem(c);
        if (hi) hi->setForeground(QColor(0xa8, 0xc0, 0xe0));
    }
    root->addWidget(m_table, 1);

    m_hintLabel = new QLabel(
        QStringLiteral("主板块优先取行业分类；相对强度 = 个股涨幅 − 板块涨幅"), this);
    m_hintLabel->setStyleSheet("color:#8fa3c0;font-size:11px;");
    root->addWidget(m_hintLabel);

    setPending(QStringLiteral("正在获取板块数据…"));
}

void SectorDialog::setPending(const QString &hint)
{
    m_titleLabel->setText(hint);
    m_verdictLabel->clear();
    m_relLabel->clear();
    m_table->setRowCount(0);
}

void SectorDialog::setLink(const SectorLink &link)
{
    // ---- 顶部：主板块摘要 ----
    if (link.hasPrimary) {
        // 注意：占位符与 arg() 调用必须一一对应，否则 QString::arg 会报警并丢字
        const QString stockTxt = QStringLiteral("%1%2")
                                     .arg(link.stockPct >= 0 ? QStringLiteral("+")
                                                             : QString())
                                     .arg(link.stockPct, 0, 'f', 2);
        const QString sectorTxt = QStringLiteral("%1%2")
                                      .arg(link.primary.changePct >= 0
                                               ? QStringLiteral("+") : QString())
                                      .arg(link.primary.changePct, 0, 'f', 2);
        m_titleLabel->setText(
            QStringLiteral("主板块：%1（%2）　个股 %3%　板块 %4%")
                .arg(link.primary.name,
                     SectorCalc::kindName(link.primary.kind),
                     stockTxt, sectorTxt));
    } else {
        m_titleLabel->setText(
            QStringLiteral("未获取到所属板块（该股可能停牌，或为指数）"));
    }

    // ---- 相对强度结论 ----
    QColor lv = C_FLAT;
    switch (link.level) {
    case StrengthLevel::Leader:
    case StrengthLevel::SlightlyUp:   lv = C_UP;   break;
    case StrengthLevel::SlightlyDown:
    case StrengthLevel::Laggard:      lv = C_DOWN; break;
    default: break;
    }

    m_verdictLabel->setStyleSheet(
        QStringLiteral("font-size:13px;font-weight:bold;color:%1;"
                       "border:none;background:transparent;").arg(lv.name()));
    m_verdictLabel->setText(
        QStringLiteral("%1：%2")
            .arg(SectorCalc::levelName(link.level), link.verdict));

    if (link.hasPrimary) {
        m_relLabel->setStyleSheet(
            QStringLiteral("font-size:12px;color:%1;border:none;background:transparent;")
                .arg(lv.name()));
        m_relLabel->setText(
            QStringLiteral("相对强度 %1%2 个百分点")
                .arg(link.relStrength >= 0 ? QStringLiteral("+") : QString())
                .arg(link.relStrength, 0, 'f', 2));
    } else {
        m_relLabel->clear();
    }

    // ---- 板块列表（按涨幅降序，主板块加星并高亮）----
    m_table->setRowCount(link.sectors.size());
    for (int i = 0; i < link.sectors.size(); ++i) {
        const SectorItem &s = link.sectors[i];
        const bool isPrimary = link.hasPrimary && s.code == link.primary.code;
        const QColor c = s.changePct > 0 ? C_UP
                                       : (s.changePct < 0 ? C_DOWN : C_FLAT);

        auto setCell = [&](int col, const QString &text, const QColor &fg,
                           Qt::Alignment al) {
            auto *it = new QTableWidgetItem(text);
            it->setTextAlignment(al);
            it->setForeground(fg);
            if (isPrimary) {
                QFont f = it->font();
                f.setBold(true);
                it->setFont(f);
            }
            m_table->setItem(i, col, it);
        };

        setCell(0, SectorCalc::kindName(s.kind), C_FLAT, Qt::AlignCenter);
        setCell(1, isPrimary ? QStringLiteral("★ %1").arg(s.name) : s.name,
                isPrimary ? QColor(0x4d, 0xa3, 0xff) : QColor(0xe8, 0xee, 0xfc),
                Qt::AlignLeft | Qt::AlignVCenter);
        setCell(2, QStringLiteral("%1%2%")
                       .arg(s.changePct > 0 ? QStringLiteral("+") : QString())
                       .arg(s.changePct, 0, 'f', 2),
                c, Qt::AlignRight | Qt::AlignVCenter);
    }

    if (link.sectors.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("该股暂无板块归属数据"));
    }
}
