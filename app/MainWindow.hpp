// Main application window: editor + docks + toolbar actions.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include "casl/Assembler.hpp"
#include "casl/Machine.hpp"

#include <QMainWindow>

#include <string>

class CCodeEditor;
class CMachineRunner;
class CRegistersPanel;
class CMemoryPanel;
class CSymbolsPanel;
class CConsolePanel;
class CErrorsPanel;
class CCommandHelpDialog;
class QLabel;

class CMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit CMainWindow(QWidget* pParent = nullptr);

    // ---- test hooks (used by tests/test_gui.cpp) ---------------------------
    CMachineRunner* GetRunnerForTest() const;
    CRegistersPanel* GetRegistersPanelForTest() const { return m_pRegistersPanel; }
    void SetSourceForTest(const QString& strText);
    void TriggerAssembleForTest() { OnAssemble(); }
    void TriggerStepForTest() { OnStep(); }

protected:
    void closeEvent(QCloseEvent* pEvent) override;

private slots:
    void OnFileNew();
    void OnFileOpen();
    bool OnFileSave();
    bool OnFileSaveAs();
    void OnAssemble();
    void OnRun();
    void OnStep();
    void OnStop();
    void OnResetAndRun();
    void OnToggleBreakpoint();
    void OnHelpCommands();       // help menu (F1)
    void ShowHelpDialog(const QString& strKeyword = QString());
    void OnStateChanged(const casl::CMachineState& state);
    void OnOutput(const QString& strText);
    void OnInputNeeded();
    void OnRunFinished(bool bClean, const QString& strError);
    void OnErrorSelected(int nLine);
    void OnBreakpointsChanged();
    void OnSampleMenu();

private:
    void CreateActions();
    void CreatePanels();
    void UpdateActions(const casl::CMachineState& state);
    bool ConfirmSaveChanges();
    int LineForAddress(int nAddress) const;
    void PushBreakpointsToRunner();
    QIcon MakeVsIcon(const QString& strKind) const;

    // editor + runner
    CCodeEditor* m_pEditor = nullptr;
    CMachineRunner* m_pRunner = nullptr;

    // panels
    CRegistersPanel* m_pRegistersPanel = nullptr;
    CMemoryPanel* m_pMemoryPanel = nullptr;
    CSymbolsPanel* m_pSymbolsPanel = nullptr;
    CConsolePanel* m_pConsolePanel = nullptr;
    CErrorsPanel* m_pErrorsPanel = nullptr;
    CCommandHelpDialog* m_pHelpDlg = nullptr; // created lazily, non-modal

    // actions
    QAction* m_pactNew = nullptr;
    QAction* m_pactOpen = nullptr;
    QAction* m_pactSave = nullptr;
    QAction* m_pactAssemble = nullptr;
    QAction* m_pactRun = nullptr;       // F5  开始调试/继续
    QAction* m_pactRunNoDebug = nullptr; // Ctrl+F5 开始执行(不调试)
    QAction* m_pactStep = nullptr;      // F10 逐过程
    QAction* m_pactStepInto = nullptr;  // F11 逐语句
    QAction* m_pactStepOut = nullptr;   // Shift+F11 跳出
    QAction* m_pactStop = nullptr;      // Shift+F5 停止
    QAction* m_pactResetRun = nullptr;  // Ctrl+Shift+F5 重新调试
    QAction* m_pactBreakpoint = nullptr; // F9 切换断点

    // state
    QString m_strFilePath;
    casl::CAssembleResult m_result;
    bool m_bAssembledOk = false;
    QLabel* m_plblStatus = nullptr;
};
