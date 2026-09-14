#include "Panels.hpp"

#include "casl/Machine.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// CRegistersPanel
// ---------------------------------------------------------------------------

static QString WordToHex(uint16_t w) {
    return QString("%1").arg(w, 4, 16, QChar('0')).toUpper();
}

CRegistersPanel::CRegistersPanel(QWidget* pParent) : QWidget(pParent) {
    auto* pGrid = new QGridLayout(this);
    pGrid->setContentsMargins(8, 8, 8, 8);
    pGrid->setSpacing(4);

    // two columns of rows: GR0..GR4 | GR5..GR7, SP, PR
    pGrid->setColumnStretch(2, 1);
    pGrid->setColumnStretch(4, 1);

    // The value labels pin a light background, so the text color must be
    // pinned too: on dark-system themes the palette text is light and the
    // values would become white-on-white (invisible, "empty panel" bug).
    const QString strValueStyle =
        "font-family: Consolas, monospace;"
        "background: #F4F4F4; color: #202020; padding: 1px 6px;"
        "border: 1px solid #C8C8C8;";

    for (int i = 0; i < 10; ++i) {
        QString strName = i < 8 ? QString("GR%1").arg(i)
                                : (i == 8 ? "SP" : "PR");
        int nRow = i % 5;
        int nCol = (i / 5) * 2;
        auto* plblName = new QLabel(strName, this);
        plblName->setStyleSheet("font-weight: bold;");
        // right-align names so they hug the value boxes (esp. SP/PR vs GR5-7)
        plblName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* plblValue = new QLabel("0000", this);
        m_arrRegLabels[i] = plblValue;
        plblValue->setStyleSheet(strValueStyle);
        pGrid->addWidget(plblName, nRow, nCol);
        pGrid->addWidget(plblValue, nRow, nCol + 1);
    }

    auto* pFlagsRow = new QWidget(this);
    auto* pFlags = new QHBoxLayout(pFlagsRow);
    pFlags->setContentsMargins(0, 0, 0, 0);
    auto MakeFlag = [&](const QString& strName, QLabel** pplbl) {
        auto* plblName = new QLabel(strName, pFlagsRow);
        plblName->setStyleSheet("font-weight: bold;");
        auto* plblValue = new QLabel("0", pFlagsRow);
        plblValue->setStyleSheet("font-family: Consolas, monospace;"
                                 "background: #F4F4F4; color: #202020;"
                                 "padding: 1px 6px; border: 1px solid #C8C8C8;");
        *pplbl = plblValue;
        pFlags->addWidget(plblName);
        pFlags->addWidget(plblValue);
        pFlags->addSpacing(8);
    };
    MakeFlag("OF", &m_plblOF);
    MakeFlag("SF", &m_plblSF);
    MakeFlag("ZF", &m_plblZF);
    pFlags->addStretch(1);
    auto* plblStepsName = new QLabel("STEPS", pFlagsRow);
    plblStepsName->setStyleSheet("font-weight: bold;");
    m_plblSteps = new QLabel("0", pFlagsRow);
    m_plblSteps->setStyleSheet("font-family: Consolas, monospace;"
                               "background: #F4F4F4; color: #202020;"
                               "padding: 1px 6px; border: 1px solid #C8C8C8;");
    pFlags->addWidget(plblStepsName);
    pFlags->addWidget(m_plblSteps);
    pGrid->addWidget(pFlagsRow, 5, 0, 1, 4);

    m_plblState = new QLabel("idle", this);
    m_plblState->setStyleSheet("color: #666;");
    pGrid->addWidget(m_plblState, 6, 0, 1, 4);
    pGrid->setRowStretch(7, 1);
}

void CRegistersPanel::AddRow(const QString&, int) {
    // layout is built directly in the constructor; kept for API symmetry
}

void CRegistersPanel::UpdateState(const casl::CMachineState& state) {
    for (int i = 0; i < 8; ++i)
        m_arrRegLabels[i]->setText(WordToHex((uint16_t)state.m_arrGr[i]));
    m_arrRegLabels[8]->setText(WordToHex(state.m_wSp));
    m_arrRegLabels[9]->setText(WordToHex(state.m_wPr));
    m_plblOF->setText(state.m_bOf ? "1" : "0");
    m_plblSF->setText(state.m_bSf ? "1" : "0");
    m_plblZF->setText(state.m_bZf ? "1" : "0");
    m_plblSteps->setText(QString::number(state.m_nSteps));
    const char* pszState = "unknown";
    switch (state.m_eState) {
    case casl::ERunState::stIdle: pszState = "idle"; break;
    case casl::ERunState::stReady: pszState = "ready"; break;
    case casl::ERunState::stRunning: pszState = "running"; break;
    case casl::ERunState::stWaitingInput: pszState = "waiting for input"; break;
    case casl::ERunState::stPaused: pszState = "paused"; break;
    case casl::ERunState::stHalted: pszState = "halted"; break;
    case casl::ERunState::stError: pszState = "ERROR"; break;
    }
    m_plblState->setText(QString("state: %1").arg(pszState));
}

void CRegistersPanel::Clear() {
    casl::CMachineState empty;
    UpdateState(empty);
}

// ---------------------------------------------------------------------------
// CMemoryPanel
// ---------------------------------------------------------------------------

CMemoryPanel::CMemoryPanel(QWidget* pParent) : QWidget(pParent) {
    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(4, 4, 4, 4);

    auto* pTop = new QWidget(this);
    auto* pTopLayout = new QHBoxLayout(pTop);
    pTopLayout->setContentsMargins(0, 0, 0, 0);
    pTopLayout->addWidget(new QLabel("base:", pTop));
    m_pspinBase = new QSpinBox(pTop);
    m_pspinBase->setRange(0, 0xFFF0);
    m_pspinBase->setDisplayIntegerBase(16);
    m_pspinBase->setPrefix("0x");
    m_pspinBase->setFixedWidth(90);
    pTopLayout->addWidget(m_pspinBase);
    pTopLayout->addStretch(1);
    pLayout->addWidget(pTop);

    m_ptable = new QTableWidget(ROWS, WORDS_PER_ROW + 1, this);
    m_ptable->horizontalHeader()->setVisible(false);
    m_ptable->verticalHeader()->setVisible(false);
    m_ptable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ptable->setSelectionMode(QAbstractItemView::NoSelection);
    m_ptable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ptable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    pLayout->addWidget(m_ptable);
    Rebuild();

    connect(m_pspinBase, &QSpinBox::valueChanged, this, [this](int nValue) {
        m_nBase = nValue & ~7;
        Rebuild();
    });
}

void CMemoryPanel::Rebuild() {
    for (int nRow = 0; nRow < ROWS; ++nRow) {
        auto* pItemAddr = new QTableWidgetItem(
            QString("%1").arg(m_nBase + nRow * WORDS_PER_ROW, 4, 16, QChar('0')).toUpper());
        pItemAddr->setFlags(Qt::NoItemFlags);
        pItemAddr->setForeground(QBrush(QColor(120, 120, 120)));
        m_ptable->setItem(nRow, 0, pItemAddr);
        for (int nCol = 0; nCol < WORDS_PER_ROW; ++nCol) {
            auto* pItem = new QTableWidgetItem("0000");
            pItem->setFlags(Qt::NoItemFlags);
            m_ptable->setItem(nRow, nCol + 1, pItem);
        }
    }
}

void CMemoryPanel::UpdateMemory(const std::vector<uint16_t>& arrWords,
                                uint16_t wHighlightAddr) {
    m_wHighlightAddr = wHighlightAddr;
    if (m_pspinBase->value() != m_nBase) m_pspinBase->setValue(m_nBase);
    for (int nRow = 0; nRow < ROWS; ++nRow) {
        for (int nCol = 0; nCol < WORDS_PER_ROW; ++nCol) {
            int nAddr = m_nBase + nRow * WORDS_PER_ROW + nCol;
            QTableWidgetItem* pItem = m_ptable->item(nRow, nCol + 1);
            pItem->setText(WordToHex(arrWords[(size_t)nAddr]));
            pItem->setBackground(nAddr == (int)m_wHighlightAddr
                                     ? QBrush(QColor(255, 230, 140))
                                     : QBrush());
        }
    }
}

void CMemoryPanel::Clear() {
    std::vector<uint16_t> arrEmpty(casl::MEMORY_WORDS, 0);
    UpdateMemory(arrEmpty, 0xFFFF);
}

// ---------------------------------------------------------------------------
// CSymbolsPanel
// ---------------------------------------------------------------------------

CSymbolsPanel::CSymbolsPanel(QWidget* pParent) : QWidget(pParent) {
    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(4, 4, 4, 4);
    m_ptable = new QTableWidget(0, 2, this);
    m_ptable->setHorizontalHeaderLabels({"label", "address"});
    m_ptable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ptable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ptable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pLayout->addWidget(m_ptable);
}

void CSymbolsPanel::UpdateSymbols(const std::map<std::string, int>& mapSymbols) {
    m_ptable->setRowCount((int)mapSymbols.size());
    int nRow = 0;
    for (const auto& kv : mapSymbols) {
        auto* pName = new QTableWidgetItem(QString::fromStdString(kv.first));
        auto* pAddr = new QTableWidgetItem(
            QString("%1").arg((uint16_t)kv.second, 4, 16, QChar('0')).toUpper());
        pAddr->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_ptable->setItem(nRow, 0, pName);
        m_ptable->setItem(nRow, 1, pAddr);
        ++nRow;
    }
}

// ---------------------------------------------------------------------------
// CConsolePanel
// ---------------------------------------------------------------------------

CConsolePanel::CConsolePanel(QWidget* pParent) : QWidget(pParent) {
    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(4, 4, 4, 4);
    m_ptxtOutput = new QPlainTextEdit(this);
    m_ptxtOutput->setReadOnly(true);
    m_ptxtOutput->setFont(QFont("Consolas", 10));
    pLayout->addWidget(m_ptxtOutput, 1);

    auto* pBottom = new QWidget(this);
    auto* pBottomLayout = new QHBoxLayout(pBottom);
    pBottomLayout->setContentsMargins(0, 0, 0, 0);
    m_plblHint = new QLabel("程序输入 (IN)", pBottom);
    m_peditInput = new QLineEdit(pBottom);
    m_peditInput->setFont(QFont("Consolas", 10));
    m_peditInput->setPlaceholderText(
        "程序执行 IN 指令时，在此输入一行字符并回车（不是改内存的入口）");
    m_peditInput->setToolTip(
        "当程序运行到 IN buf,len 指令暂停等待输入时，在这里输入一行字符：\n"
        "字符逐个写入内存 buf 开始的单元（每个字符占一个字），\n"
        "实际个数写入 len 处的一个字。平时（未在等待输入）此框不可用。\n"
        "想修改程序里的数据（如 DC 定义的字符串），请直接改源程序后按 F7 重新编译。");
    m_pbtnSend = new QPushButton("送入", pBottom);
    pBottomLayout->addWidget(m_plblHint);
    pBottomLayout->addWidget(m_peditInput, 1);
    pBottomLayout->addWidget(m_pbtnSend);
    pLayout->addWidget(pBottom);

    connect(m_pbtnSend, &QPushButton::clicked, this, [this] {
        emit InputSubmitted(m_peditInput->text());
        m_peditInput->clear();
    });
    connect(m_peditInput, &QLineEdit::returnPressed, m_pbtnSend,
            &QPushButton::click);
    MarkWaitingForInput(false); // start disabled: only IN unlocks it
}

void CConsolePanel::AppendOutput(const QString& strText) {
    m_ptxtOutput->moveCursor(QTextCursor::End);
    m_ptxtOutput->insertPlainText(strText);
    m_ptxtOutput->moveCursor(QTextCursor::End);
}

void CConsolePanel::ClearOutput() {
    m_ptxtOutput->clear();
}

void CConsolePanel::MarkWaitingForInput(bool bWaiting) {
    m_plblHint->setText(bWaiting ? "程序输入 (IN) 等待中 >" : "程序输入 (IN)");
    m_plblHint->setStyleSheet(bWaiting ? "color: #B00; font-weight: bold;" : "");
    m_peditInput->setEnabled(bWaiting);
    m_pbtnSend->setEnabled(bWaiting);
    if (bWaiting) {
        m_peditInput->setFocus();
        m_peditInput->setPlaceholderText("输入一行字符后回车");
    } else {
        m_peditInput->setPlaceholderText(
            "程序执行 IN 指令时，在此输入一行字符并回车（不是改内存的入口）");
    }
}

// ---------------------------------------------------------------------------
// CErrorsPanel
// ---------------------------------------------------------------------------

CErrorsPanel::CErrorsPanel(QWidget* pParent) : QWidget(pParent) {
    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(4, 4, 4, 4);
    m_plistErrors = new QListWidget(this);
    pLayout->addWidget(m_plistErrors);
    connect(m_plistErrors, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* pItem) {
                bool bOk = false;
                int nLine = pItem->data(Qt::UserRole).toInt(&bOk);
                if (bOk && nLine > 0) emit ErrorSelected(nLine);
            });
}

void CErrorsPanel::UpdateErrors(const std::vector<casl::CAsmError>& arrErrors) {
    m_plistErrors->clear();
    for (const auto& err : arrErrors) {
        QString strText = err.m_nLine > 0
                              ? QString("line %1: %2")
                                    .arg(err.m_nLine)
                                    .arg(QString::fromStdString(err.m_strMessage))
                              : QString::fromStdString(err.m_strMessage);
        auto* pItem = new QListWidgetItem(strText);
        pItem->setData(Qt::UserRole, err.m_nLine);
        m_plistErrors->addItem(pItem);
    }
}
