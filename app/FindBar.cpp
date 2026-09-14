#include "FindBar.hpp"

#include "CodeEditor.hpp" // EditorTheme()

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QKeyEvent>
#include <QToolButton>

namespace {

QToolButton* MakeToggleBtn(const QString& strText, QWidget* pParent) {
    auto* pBtn = new QToolButton(pParent);
    pBtn->setText(strText);
    pBtn->setCheckable(true);
    pBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    pBtn->setAutoRaise(true);
    pBtn->setCursor(Qt::PointingHandCursor);
    pBtn->setStyleSheet(
        "QToolButton { font-family: Consolas, monospace; padding: 0 4px;"
        " border-radius: 3px; }"
        "QToolButton:hover { border: 1px solid palette(mid); }");
    return pBtn;
}

} // namespace

CFindBar::CFindBar(QWidget* pParent) : QWidget(pParent) {
    // theme-aware colors
    const CEditorTheme& th = EditorTheme();
    const bool bDark = th.m_clrPaper.lightness() < 128;
    const QString strBarBg = bDark ? "#2D2D30" : "#FFFFFF";
    const QString strBarBorder = bDark ? "#3F3F46" : "#C8C8C8";
    const QString strBtnActive = "#007ACC";
    const QString strBtnHover =
        bDark ? "rgba(255,255,255,0.12)" : "rgba(0,0,0,0.08)";

    setAutoFillBackground(true);
    setStyleSheet(QString(
        "CFindBar { background: transparent; border-radius: 4px; }"
        "QFrame#findFrame { background: %1; border: 1px solid %2;"
        " border-radius: 4px; }"
        "QLineEdit { background: transparent; border: none; padding: 2px 4px;"
        " color: %3; selection-background-color: %4; }")
        .arg(strBarBg, strBarBorder, th.m_clrText.name(),
             th.m_clrSelectionBack.name()));

    auto* pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(4, 2, 2, 2);
    pLayout->setSpacing(4);

    // ---- the input box with the Aa / ab toggles inside ----
    auto* pFrame = new QFrame(this);
    pFrame->setObjectName("findFrame");
    auto* pFrameLayout = new QHBoxLayout(pFrame);
    pFrameLayout->setContentsMargins(2, 1, 2, 1);
    pFrameLayout->setSpacing(2);

    m_peditFind = new QLineEdit(pFrame);
    m_peditFind->setPlaceholderText("查找");
    m_peditFind->setFrame(false);
    m_peditFind->setFixedWidth(200);
    m_peditFind->setFont(CaslCodeFont());
    m_peditFind->installEventFilter(this); // Esc / Enter / Shift+Enter
    pFrameLayout->addWidget(m_peditFind, 1);

    auto StyleToggle = [strBtnActive, strBtnHover](QToolButton* pBtn) {
        pBtn->setStyleSheet(QString(
            "QToolButton { font-family: Consolas, monospace; padding: 1px 5px;"
            " border-radius: 3px; }"
            "QToolButton:hover { background: %1; }"
            "QToolButton:checked { background: %2; color: white; }")
            .arg(strBtnHover, strBtnActive));
    };
    m_pbtnAa = MakeToggleBtn("Aa", pFrame);
    StyleToggle(m_pbtnAa);
    m_pbtnAa->setToolTip("区分大小写");
    m_pbtnAb = MakeToggleBtn("ab", pFrame);
    StyleToggle(m_pbtnAb);
    m_pbtnAb->setToolTip("全字匹配");
    pFrameLayout->addWidget(m_pbtnAa);
    pFrameLayout->addWidget(m_pbtnAb);
    pLayout->addWidget(pFrame);

    // ---- outside, right: [第N项,共M项] up down close ----
    m_plblCount = new QLabel(this);
    m_plblCount->setFont(CaslCodeFont());
    pLayout->addWidget(m_plblCount);

    auto MakeNavBtn = [&](const QString& strText, const QString& strTip) {
        auto* pBtn = new QToolButton(this);
        pBtn->setText(strText);
        pBtn->setToolTip(strTip);
        pBtn->setAutoRaise(true);
        pBtn->setStyleSheet(
            QString("QToolButton { font-family: Consolas; padding: 2px 5px;"
                    " border-radius: 3px; }"
                    "QToolButton:hover { background: %1; }")
                .arg(strBtnHover));
        return pBtn;
    };
    m_pbtnPrev = MakeNavBtn("↑", "上一个 (Shift+Enter)");
    m_pbtnNext = MakeNavBtn("↓", "下一个 (Enter)");
    m_pbtnClose = MakeNavBtn("✕", "关闭 (Esc)");
    pLayout->addWidget(m_pbtnPrev);
    pLayout->addWidget(m_pbtnNext);
    pLayout->addWidget(m_pbtnClose);

    connect(m_peditFind, &QLineEdit::textChanged, this,
            &CFindBar::FindTextChanged);
    connect(m_pbtnAa, &QToolButton::toggled, this, &CFindBar::OptionsChanged);
    connect(m_pbtnAb, &QToolButton::toggled, this, &CFindBar::OptionsChanged);
    connect(m_pbtnNext, &QToolButton::clicked, this, &CFindBar::NextRequested);
    connect(m_pbtnPrev, &QToolButton::clicked, this, &CFindBar::PrevRequested);
    connect(m_pbtnClose, &QToolButton::clicked, this,
            &CFindBar::CloseRequested);

    UpdateCount(-1, 0);
    hide();
}

bool CFindBar::IsCaseSensitive() const { return m_pbtnAa->isChecked(); }
bool CFindBar::IsWholeWord() const { return m_pbtnAb->isChecked(); }

void CFindBar::ShowAndFocus() {
    show();
    raise();
    m_peditFind->setFocus();
    m_peditFind->selectAll();
}

QString CFindBar::MakeCountText(int nCur, int nTotal) const {
    if (nTotal <= 0) return QStringLiteral("共 0 项");
    // nCur is 0-based; show as 1-based 第 N 项
    return QStringLiteral("第 %1 项,共 %2 项").arg(nCur + 1).arg(nTotal);
}

void CFindBar::UpdateCount(int nCur, int nTotal) {
    m_plblCount->setText(MakeCountText(nCur, nTotal));
    m_plblCount->setStyleSheet(
        nTotal > 0 ? QString()
                   : QStringLiteral("color: #E07070;")); // red "no matches"
}

bool CFindBar::eventFilter(QObject* pWatched, QEvent* pEvent) {
    if (pWatched == m_peditFind && pEvent->type() == QEvent::KeyPress) {
        auto* pKey = static_cast<QKeyEvent*>(pEvent);
        if (pKey->key() == Qt::Key_Escape) {
            emit CloseRequested();
            return true;
        }
        if (pKey->key() == Qt::Key_Return || pKey->key() == Qt::Key_Enter) {
            if (pKey->modifiers() & Qt::ShiftModifier) emit PrevRequested();
            else emit NextRequested();
            return true;
        }
    }
    return QWidget::eventFilter(pWatched, pEvent);
}
