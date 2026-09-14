#include "MainWindow.hpp"

#include "CodeEditor.hpp"
#include "CommandHelp.hpp"
#include "MachineRunner.hpp"
#include "Panels.hpp"

#include "casl/Machine.hpp"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>
#include <QUrl>

CMainWindow::CMainWindow(QWidget* pParent) : QMainWindow(pParent) {
    setWindowTitle("CASL Studio - CASL/COMET II 汇编开发环境");
    resize(1280, 800);

    m_pEditor = new CCodeEditor(this);
    setCentralWidget(m_pEditor);

    m_pRunner = new CMachineRunner(this);

    CreatePanels();
    CreateActions();

    m_plblStatus = new QLabel("ready", this);
    statusBar()->addWidget(m_plblStatus);

    connect(m_pRunner, &CMachineRunner::StateChanged, this,
            &CMainWindow::OnStateChanged);
    connect(m_pRunner, &CMachineRunner::OutputReady, this,
            &CMainWindow::OnOutput);
    connect(m_pRunner, &CMachineRunner::InputNeeded, this,
            &CMainWindow::OnInputNeeded);
    connect(m_pRunner, &CMachineRunner::RunFinished, this,
            &CMainWindow::OnRunFinished);
    connect(m_pConsolePanel, &CConsolePanel::InputSubmitted, m_pRunner,
            &CMachineRunner::ProvideInput);
    connect(m_pErrorsPanel, &CErrorsPanel::ErrorSelected, this,
            &CMainWindow::OnErrorSelected);
    connect(m_pEditor, &CCodeEditor::BreakpointsChanged, this,
            &CMainWindow::OnBreakpointsChanged);

    // start with a small template
    m_pEditor->SetSourceText(
        "; CASL II 程序模板\n"
        "MAIN    START\n"
        "        OUT   MSG,LNG\n"
        "        EXIT\n"
        "MSG     DC    'HELLO, CASL!'\n"
        "LNG     DC    13\n"
        "        END\n");
    OnAssemble();
}

// ---------------------------------------------------------------------------
// VS-style icons drawn programmatically (green play, red square, yellow arrow
// over brackets, red breakpoint dot) so no resource files are needed.
// ---------------------------------------------------------------------------

QIcon CMainWindow::MakeVsIcon(const QString& strKind) const {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    QPolygonF poly;

    if (strKind == "run") {
        painter.setBrush(QBrush(QColor(0, 128, 0)));
        painter.setPen(Qt::NoPen);
        poly << QPointF(4, 2) << QPointF(14, 8) << QPointF(4, 14);
        painter.drawPolygon(poly);
    } else if (strKind == "stop") {
        painter.setBrush(QBrush(QColor(180, 30, 30)));
        painter.setPen(Qt::NoPen);
        painter.drawRect(3, 3, 10, 10);
    } else if (strKind == "restart") {
        painter.setBrush(QBrush(QColor(0, 110, 190)));
        painter.setPen(QPen(QColor(0, 110, 190), 2));
        painter.drawArc(2, 2, 12, 12, 30 * 16, 280 * 16);
        painter.setPen(Qt::NoPen);
        poly << QPointF(13, 1) << QPointF(16, 7) << QPointF(10, 7);
        painter.drawPolygon(poly);
    } else if (strKind == "stepover" || strKind == "stepinto") {
        // yellow curved arrow (step); "stepinto" bends downward
        painter.setPen(QPen(QColor(190, 140, 0), 2));
        QPainterPath path;
        if (strKind == "stepover") {
            path.moveTo(2, 11);
            path.quadTo(4, 2, 13, 6);
            painter.drawPath(path);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(QColor(190, 140, 0)));
            poly << QPointF(14, 3) << QPointF(16, 9) << QPointF(10, 8);
        } else {
            path.moveTo(3, 3);
            path.quadTo(12, 3, 11, 11);
            painter.drawPath(path);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(QColor(190, 140, 0)));
            poly << QPointF(8, 10) << QPointF(14, 11) << QPointF(10, 16);
        }
        painter.drawPolygon(poly);
    } else if (strKind == "stepout") {
        // yellow arrow leaving brackets
        painter.setPen(QPen(QColor(120, 120, 120), 2));
        painter.drawLine(2, 4, 2, 12);
        painter.drawLine(14, 4, 14, 12);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(190, 140, 0)));
        poly << QPointF(5, 7) << QPointF(12, 8) << QPointF(5, 10);
        painter.drawPolygon(poly);
    } else if (strKind == "breakpoint") {
        painter.setBrush(QBrush(QColor(200, 30, 30)));
        painter.setPen(QPen(QColor(120, 0, 0), 1));
        painter.drawEllipse(3, 3, 10, 10);
    } else if (strKind == "assemble") {
        painter.setPen(QPen(QColor(0, 110, 190), 2));
        painter.drawRect(2, 6, 4, 8);
        painter.drawRect(7, 3, 4, 11);
        painter.drawRect(12, 8, 3, 6);
    }
    return QIcon(pm);
}

void CMainWindow::CreateActions() {
    QToolBar* pToolBar = addToolBar("main");
    pToolBar->setMovable(false);
    pToolBar->setIconSize(QSize(16, 16));

    m_pactNew = new QAction("新建", this);
    m_pactNew->setShortcut(QKeySequence::New);
    m_pactOpen = new QAction("打开...", this);
    m_pactOpen->setShortcut(QKeySequence::Open);
    m_pactSave = new QAction("保存", this);
    m_pactSave->setShortcut(QKeySequence::Save);
    m_pactAssemble = new QAction("汇编", this);
    m_pactAssemble->setShortcut(Qt::Key_F7);            // VS: 生成 F7
    m_pactAssemble->setIcon(MakeVsIcon("assemble"));
    m_pactResetRun = new QAction("重新调试", this);
    m_pactResetRun->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));
    m_pactResetRun->setIcon(MakeVsIcon("restart"));
    m_pactRun = new QAction("开始调试 / 继续", this);
    m_pactRun->setShortcut(Qt::Key_F5);                 // VS: F5
    m_pactRun->setIcon(MakeVsIcon("run"));
    m_pactRunNoDebug = new QAction("开始执行(不调试)", this);
    m_pactRunNoDebug->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F5));
    m_pactRunNoDebug->setIcon(MakeVsIcon("run"));
    m_pactStep = new QAction("逐过程", this);
    m_pactStep->setShortcut(Qt::Key_F10);               // VS: F10
    m_pactStep->setIcon(MakeVsIcon("stepover"));
    m_pactStepInto = new QAction("逐语句", this);
    m_pactStepInto->setShortcut(Qt::Key_F11);           // VS: F11
    m_pactStepInto->setIcon(MakeVsIcon("stepinto"));
    m_pactStepOut = new QAction("跳出", this);
    m_pactStepOut->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11));
    m_pactStepOut->setIcon(MakeVsIcon("stepout"));
    m_pactStop = new QAction("停止调试", this);
    m_pactStop->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5)); // VS: Shift+F5
    m_pactStop->setIcon(MakeVsIcon("stop"));
    m_pactBreakpoint = new QAction("切换断点", this);
    m_pactBreakpoint->setShortcut(Qt::Key_F9);          // VS: F9
    m_pactBreakpoint->setIcon(MakeVsIcon("breakpoint"));

    connect(m_pactNew, &QAction::triggered, this, &CMainWindow::OnFileNew);
    connect(m_pactOpen, &QAction::triggered, this, &CMainWindow::OnFileOpen);
    connect(m_pactSave, &QAction::triggered, this, &CMainWindow::OnFileSave);
    connect(m_pactAssemble, &QAction::triggered, this, &CMainWindow::OnAssemble);
    connect(m_pactResetRun, &QAction::triggered, this, &CMainWindow::OnResetAndRun);
    connect(m_pactRun, &QAction::triggered, this, &CMainWindow::OnRun);
    connect(m_pactRunNoDebug, &QAction::triggered, this, [this] {
        if (!m_bAssembledOk) return;
        m_pConsolePanel->ClearOutput();
        m_pRunner->ResetMachine();
        m_pRunner->StartRunNoDebug();
    });
    connect(m_pactStep, &QAction::triggered, this, &CMainWindow::OnStep);
    connect(m_pactStepInto, &QAction::triggered, this, &CMainWindow::OnStep);
    connect(m_pactStepOut, &QAction::triggered, m_pRunner,
            &CMachineRunner::StartStepOut);
    connect(m_pactStop, &QAction::triggered, this, &CMainWindow::OnStop);
    connect(m_pactBreakpoint, &QAction::triggered, this,
            &CMainWindow::OnToggleBreakpoint);

    pToolBar->addAction(m_pactNew);
    pToolBar->addAction(m_pactOpen);
    pToolBar->addAction(m_pactSave);
    pToolBar->addSeparator();
    pToolBar->addAction(m_pactAssemble);
    pToolBar->addSeparator();
    pToolBar->addAction(m_pactResetRun);
    pToolBar->addAction(m_pactRun);
    pToolBar->addAction(m_pactRunNoDebug);
    pToolBar->addAction(m_pactStep);
    pToolBar->addAction(m_pactStepInto);
    pToolBar->addAction(m_pactStepOut);
    pToolBar->addAction(m_pactStop);
    pToolBar->addSeparator();
    pToolBar->addAction(m_pactBreakpoint);

    QMenu* pFileMenu = menuBar()->addMenu("文件(&F)");
    pFileMenu->addAction(m_pactNew);
    pFileMenu->addAction(m_pactOpen);
    pFileMenu->addAction(m_pactSave);
    QMenu* pDebugMenu = menuBar()->addMenu("调试(&D)");
    pDebugMenu->addAction(m_pactAssemble);
    pDebugMenu->addSeparator();
    pDebugMenu->addAction(m_pactResetRun);
    pDebugMenu->addAction(m_pactRun);
    pDebugMenu->addAction(m_pactRunNoDebug);
    pDebugMenu->addAction(m_pactStep);
    pDebugMenu->addAction(m_pactStepInto);
    pDebugMenu->addAction(m_pactStepOut);
    pDebugMenu->addAction(m_pactStop);
    pDebugMenu->addSeparator();
    pDebugMenu->addAction(m_pactBreakpoint);
    QMenu* pSampleMenu = menuBar()->addMenu("示例(&S)");
    QAction* pactSamples = new QAction("打开示例目录", this);
    connect(pactSamples, &QAction::triggered, this, &CMainWindow::OnSampleMenu);
    pSampleMenu->addAction(pactSamples);

    QMenu* pHelpMenu = menuBar()->addMenu("帮助(&H)");
    QAction* pactHelp = new QAction("CASL 命令帮助", this);
    pactHelp->setShortcut(Qt::Key_F1); // VS: F1 context help
    connect(pactHelp, &QAction::triggered, this, &CMainWindow::OnHelpCommands);
    pHelpMenu->addAction(pactHelp);
    QAction* pactHelpCur = new QAction("当前命令帮助 (光标处)", this);
    connect(pactHelpCur, &QAction::triggered, this, [this] {
        // same dialog, but locate the command under the editor cursor
        ShowHelpDialog(m_pEditor->CurrentWord());
    });
    pHelpMenu->addAction(pactHelpCur);

    m_pactRun->setEnabled(false);
    m_pactRunNoDebug->setEnabled(false);
    m_pactStep->setEnabled(false);
    m_pactStepInto->setEnabled(false);
    m_pactStepOut->setEnabled(false);
    m_pactStop->setEnabled(false);
    m_pactResetRun->setEnabled(false);
}

void CMainWindow::CreatePanels() {
    auto MakeDock = [this](const QString& strTitle, Qt::DockWidgetArea eArea,
                           QWidget* pWidget) {
        auto* pDock = new QDockWidget(strTitle, this);
        pDock->setWidget(pWidget);
        pDock->setFeatures(QDockWidget::DockWidgetMovable |
                           QDockWidget::DockWidgetFloatable);
        addDockWidget(eArea, pDock);
        return pDock;
    };

    m_pRegistersPanel = new CRegistersPanel(this);
    MakeDock("寄存器", Qt::RightDockWidgetArea, m_pRegistersPanel);

    m_pMemoryPanel = new CMemoryPanel(this);
    MakeDock("内存", Qt::RightDockWidgetArea, m_pMemoryPanel);

    m_pSymbolsPanel = new CSymbolsPanel(this);
    MakeDock("符号表", Qt::RightDockWidgetArea, m_pSymbolsPanel);

    m_pConsolePanel = new CConsolePanel(this);
    MakeDock("输入 / 输出", Qt::BottomDockWidgetArea, m_pConsolePanel);

    m_pErrorsPanel = new CErrorsPanel(this);
    MakeDock("汇编信息", Qt::BottomDockWidgetArea, m_pErrorsPanel);
}

// ---------------------------------------------------------------------------
// file actions
// ---------------------------------------------------------------------------

void CMainWindow::OnFileNew() {
    if (!ConfirmSaveChanges()) return;
    m_strFilePath.clear();
    m_pEditor->SetSourceText("");
    m_pConsolePanel->ClearOutput();
    setWindowTitle("CASL Studio - 未命名");
}

void CMainWindow::OnFileOpen() {
    if (!ConfirmSaveChanges()) return;
    QString strPath = QFileDialog::getOpenFileName(
        this, "打开 CASL 源程序", QString(), "CASL 程序 (*.casl *.csl *.txt);;所有文件 (*)");
    if (strPath.isEmpty()) return;
    QFile file(strPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "打开失败", "无法打开文件: " + strPath);
        return;
    }
    QTextStream strm(&file);
    QString strText = strm.readAll();
    m_strFilePath = strPath;
    m_pEditor->SetSourceText(strText);
    setWindowTitle("CASL Studio - " + QFileInfo(strPath).fileName());
    m_pConsolePanel->ClearOutput();
    OnAssemble();
}

bool CMainWindow::OnFileSave() {
    if (m_strFilePath.isEmpty()) return OnFileSaveAs();
    QFile file(m_strFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "保存失败", "无法写入文件: " + m_strFilePath);
        return false;
    }
    QTextStream strm(&file);
    strm << m_pEditor->GetSourceText();
    return true;
}

bool CMainWindow::OnFileSaveAs() {
    QString strPath = QFileDialog::getSaveFileName(
        this, "保存 CASL 源程序", QString(), "CASL 程序 (*.casl);;所有文件 (*)");
    if (strPath.isEmpty()) return false;
    m_strFilePath = strPath;
    setWindowTitle("CASL Studio - " + QFileInfo(strPath).fileName());
    return OnFileSave();
}

bool CMainWindow::ConfirmSaveChanges() {
    // keep it simple: the editor is small, we just save nothing automatically
    return true;
}

void CMainWindow::OnSampleMenu() {
    QString strDir = QCoreApplication::applicationDirPath() + "/samples";
    if (!QDir(strDir).exists())
        strDir = QApplication::applicationDirPath() + "/../samples";
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::cleanPath(strDir)));
}

// ---------------------------------------------------------------------------
// assemble / debug actions
// ---------------------------------------------------------------------------

void CMainWindow::OnAssemble() {
    m_pRunner->RequestStop();
    m_result = casl::Assemble(m_pEditor->GetSourceText().toStdString());
    m_bAssembledOk = m_result.m_bOk;
    m_pErrorsPanel->UpdateErrors(m_result.m_arrErrors);
    m_pSymbolsPanel->UpdateSymbols(m_result.m_mapSymbols);

    if (!m_result.m_bOk) {
        m_plblStatus->setText("汇编失败，请查看“汇编信息”面板");
        m_pactRun->setEnabled(false);
        m_pactRunNoDebug->setEnabled(false);
        m_pactStep->setEnabled(false);
        m_pactStepInto->setEnabled(false);
        m_pactStepOut->setEnabled(false);
        m_pactResetRun->setEnabled(false);
        return;
    }
    m_pRunner->LoadProgram(m_result);
    m_pactRun->setEnabled(true);
    m_pactRunNoDebug->setEnabled(true);
    m_pactStep->setEnabled(true);
    m_pactStepInto->setEnabled(true);
    m_pactStepOut->setEnabled(true);
    m_pactResetRun->setEnabled(true);
    m_plblStatus->setText(QString("汇编成功：%1 个字，入口 0x%2")
                              .arg(m_result.m_arrWords.size())
                              .arg(m_result.m_nStartAddress, 4, 16, QChar('0')));
    PushBreakpointsToRunner();
}

void CMainWindow::OnResetAndRun() {
    if (!m_bAssembledOk) return;
    m_pConsolePanel->ClearOutput();
    m_pRunner->ResetMachine();
    m_pRunner->StartRun();
}

void CMainWindow::OnRun() {
    if (!m_bAssembledOk) return;
    m_pRunner->StartRun();
}

void CMainWindow::OnStep() {
    if (!m_bAssembledOk) return;
    m_pRunner->StepOnce();
}

void CMainWindow::OnStop() {
    m_pRunner->RequestStop();
}

void CMainWindow::OnToggleBreakpoint() {
    m_pEditor->ToggleBreakpoint(m_pEditor->CurrentLine());
}

// ---------------------------------------------------------------------------
// runner events
// ---------------------------------------------------------------------------

int CMainWindow::LineForAddress(int nAddress) const {
    // pick the entry with the greatest start address <= nAddress that is not
    // data, i.e. the instruction currently executing
    int nBestLine = -1;
    int nBestAddr = -1;
    for (const auto& entry : m_result.m_arrEntries) {
        if (entry.m_bIsData) continue;
        if (entry.m_nAddress <= nAddress && entry.m_nAddress >= nBestAddr) {
            nBestAddr = entry.m_nAddress;
            nBestLine = entry.m_nSrcLine;
        }
    }
    return nBestLine;
}

void CMainWindow::OnStateChanged(const casl::CMachineState& state) {
    m_pRegistersPanel->UpdateState(state);

    std::vector<uint16_t> arrWords;
    m_pRunner->CopyMemory(arrWords);
    m_pMemoryPanel->UpdateMemory(arrWords, state.m_wPr);

    int nLine = LineForAddress((int)state.m_wPr);
    m_pEditor->HighlightLine(nLine);

    bool bBusy = state.m_eState == casl::ERunState::stRunning ||
                 state.m_eState == casl::ERunState::stWaitingInput;
    m_pactStep->setEnabled(!bBusy && m_bAssembledOk);
    m_pactStepInto->setEnabled(!bBusy && m_bAssembledOk);
    m_pactStepOut->setEnabled(!bBusy && m_bAssembledOk);
    m_pactRun->setEnabled(!bBusy && m_bAssembledOk);
    m_pactRunNoDebug->setEnabled(!bBusy && m_bAssembledOk);
    m_pactStop->setEnabled(bBusy);
    m_pactAssemble->setEnabled(!bBusy);
}

void CMainWindow::OnOutput(const QString& strText) {
    m_pConsolePanel->AppendOutput(strText);
}

void CMainWindow::OnInputNeeded() {
    m_pConsolePanel->MarkWaitingForInput(true);
}

void CMainWindow::OnRunFinished(bool bClean, const QString& strError) {
    m_pConsolePanel->MarkWaitingForInput(false);
    if (bClean) {
        m_plblStatus->setText("程序已正常结束 (EXIT)");
    } else {
        m_plblStatus->setText("运行错误: " + strError);
        QMessageBox::warning(this, "运行错误", strError);
    }
}

void CMainWindow::OnErrorSelected(int nLine) {
    m_pEditor->GoToLine(nLine);
}

void CMainWindow::OnBreakpointsChanged() {
    PushBreakpointsToRunner();
}

void CMainWindow::PushBreakpointsToRunner() {
    if (!m_bAssembledOk) return;
    QSet<int> setAddresses;
    // editor exposes breakpoints via a type-specific API; collect lines
    // through the public methods available on both backends.
    int nLines = m_pEditor->GetSourceText().count('\n') + 1;
    for (int nLine = 1; nLine <= nLines; ++nLine) {
        if (m_pEditor->IsBreakpointSet(nLine)) {
            for (const auto& pairLine : m_result.m_arrLineMap) {
                if (pairLine.first == nLine) {
                    setAddresses.insert(pairLine.second);
                    break;
                }
            }
        }
    }
    m_pRunner->SetBreakpoints(setAddresses);
}

void CMainWindow::OnHelpCommands() {
    // F1: open help located at the command under the editor cursor (if any)
    ShowHelpDialog(m_pEditor->CurrentWord());
}

// ---------------------------------------------------------------------------
// test hooks
// ---------------------------------------------------------------------------

CMachineRunner* CMainWindow::GetRunnerForTest() const { return m_pRunner; }

void CMainWindow::SetSourceForTest(const QString& strText) {
    m_pEditor->SetSourceText(strText);
}

void CMainWindow::ShowHelpDialog(const QString& strKeyword) {
    if (!m_pHelpDlg) m_pHelpDlg = new CCommandHelpDialog(this);
    if (!strKeyword.isEmpty()) m_pHelpDlg->LocateCommand(strKeyword);
    m_pHelpDlg->show();
    m_pHelpDlg->raise();
    m_pHelpDlg->activateWindow();
}

void CMainWindow::closeEvent(QCloseEvent* pEvent) {
    m_pRunner->RequestStop();
    QMainWindow::closeEvent(pEvent);
}
