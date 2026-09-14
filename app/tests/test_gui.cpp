// Offscreen GUI test: drives the real CMainWindow (actions, docks, panels,
// runner connections, event loop) through assemble + single-step and reads the
// register panel labels directly. Catches UI-side breakage that test_runner
// (runner-only) cannot see. Run with QT_QPA_PLATFORM=offscreen.
//
// Exit code 0 = all checks passed.

#include "MainWindow.hpp"
#include "MachineRunner.hpp"
#include "Panels.hpp"
#include "CodeEditor.hpp"
#include "FindBar.hpp"

#include <QLineEdit>

#include "casl/Machine.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QTimer>
#include <QThread>

#include <vector>

#include <cstdio>

using casl::ERunState;

namespace {

int s_nPassed = 0, s_nFailed = 0;

void Check(bool bOk, const char* pszWhat) {
    if (bOk) {
        ++s_nPassed;
        std::printf("[PASS] %s\n", pszWhat);
    } else {
        ++s_nFailed;
        std::printf("[FAIL] %s\n", pszWhat);
    }
}

template <typename TPred>
bool Pump(TPred pred, int nTimeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < nTimeoutMs) {
        if (pred()) return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return pred();
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    CMainWindow wnd;
    wnd.show();

    // give the ctor-time OnAssemble (template program) time to load
    Check(Pump([&] { return wnd.GetRunnerForTest()->GetSnapshot().m_eState ==
                            ERunState::stReady; },
               3000),
          "template assembled + loaded at startup");

    // replace the template with a program that visibly changes registers
    wnd.SetSourceForTest(
        "MAIN    START\n"
        "        LD    GR0,#1234\n"
        "        LD    GR1,#0BEEF\n" // literal > 16 bits? #BEEF fits
        "        EXIT\n"
        "        END\n");
    wnd.TriggerAssembleForTest();
    Check(Pump([&] { return wnd.GetRunnerForTest()->GetSnapshot().m_eState ==
                            ERunState::stReady; },
               3000),
          "register test program loaded");

    // single-step: LD, LD execute; the labels are the user-visible state, so
    // pump until the PANEL ITSELF shows the expected values (this also
    // verifies the queued StateChanged delivery end-to-end)
    CRegistersPanel* pPanel = wnd.GetRegistersPanelForTest();
    wnd.TriggerStepForTest();
    Check(Pump([&] { return pPanel->GetRegisterTextForTest(0) == "1234"; },
               2000),
          "step 1: panel GR0 label shows 1234");
    wnd.TriggerStepForTest();
    Check(Pump([&] { return pPanel->GetRegisterTextForTest(1) == "BEEF"; },
               2000),
          "step 2: panel GR1 label shows BEEF");
    Check(Pump([&] { return pPanel->GetRegisterTextForTest(9) == "0004"; },
               2000),
          "step 2: panel PR label shows 0004");

    // read the register panel labels the way the user sees them
    QString strGr0 = pPanel->GetRegisterTextForTest(0);
    QString strGr1 = pPanel->GetRegisterTextForTest(1);
    Check(strGr0 == "1234", "GR0 label shows 1234 after stepping");
    Check(strGr1 == "BEEF", "GR1 label shows BEEF after stepping");

    // third press executes EXIT and halts the machine
    wnd.TriggerStepForTest();
    Check(Pump([&] { return wnd.GetRunnerForTest()->GetSnapshot().m_eState ==
                            ERunState::stHalted; },
               2000),
          "third step (EXIT) halts cleanly");

    // hover tooltip text: register + symbol lookups
    Check(wnd.MakeTooltipTextForTest("GR0") ==
              QString("GR0 = 0x1234 (4660)"),
          "tooltip: GR0 shows value");
    Check(wnd.MakeTooltipTextForTest("gr1").startsWith("GR1 = 0x"),
          "tooltip: lowercase register resolved");
    Check(wnd.MakeTooltipTextForTest("PR").startsWith("PR = 0x"),
          "tooltip: PR shows value");
    Check(wnd.MakeTooltipTextForTest("MAIN").contains("= 0x0000"),
          "tooltip: symbol MAIN shows address");
    Check(wnd.MakeTooltipTextForTest("FOO") == QString(),
          "tooltip: unknown word suppressed");

    // ---- find bar ----
    CCodeEditor* pEditor = wnd.GetEditorForTest();
    pEditor->ShowFindBar();
    CFindBar* pFindBar = pEditor->GetFindBarForTest();
    Check(pFindBar != nullptr && pFindBar->isVisible(),
          "find bar shows via API");
    // right edge of the bar hugs the editor's right edge (top-right corner)
    Check(pFindBar->x() + pFindBar->width() <= pEditor->width() + 6 &&
              pFindBar->x() + pFindBar->width() >= pEditor->width() - 60,
          "find bar positioned at the editor's top-right");
    std::printf("  [info] bar x=%d w=%d editor w=%d\n", pFindBar->x(),
                pFindBar->width(), pEditor->width());
    QLineEdit* peditFind = pFindBar->findChild<QLineEdit*>();
    peditFind->setText("LD");
    Check(pEditor->GetFindMatchCountForTest() == 2,
          "find: LD matches twice in 3-line program");
    Check(pEditor->GetFindIndexForTest() == 0, "find: current index 0");
    // navigate next twice -> wraps back to 0
    emit pFindBar->NextRequested();
    Check(pEditor->GetFindIndexForTest() == 1, "find: next goes to index 1");
    emit pFindBar->NextRequested();
    Check(pEditor->GetFindIndexForTest() == 0, "find: next wraps to index 0");
    emit pFindBar->PrevRequested();
    Check(pEditor->GetFindIndexForTest() == 1,
          "find: prev wraps to last match");
    // case sensitive halves the matches (ld lowercase absent)
    pFindBar->SetCaseSensitiveForTest(true);
    Check(pEditor->GetFindMatchCountForTest() == 2,
          "find: Aa toggle keeps LD matches");
    peditFind->setText("ld");
    Check(pEditor->GetFindMatchCountForTest() == 0,
          "find: case-sensitive ld = no matches");
    pFindBar->SetCaseSensitiveForTest(false);
    Check(pEditor->GetFindMatchCountForTest() == 2,
          "find: case-insensitive ld = 2 matches");
    // whole-word: 'LD' is a whole word on both lines -> still 2
    pFindBar->SetWholeWordForTest(true);
    Check(pEditor->GetFindMatchCountForTest() == 2,
          "find: whole-word LD = 2 matches");
    peditFind->setText("L");
    Check(pEditor->GetFindMatchCountForTest() == 0,
          "find: whole-word L = no matches (LD is longer)");
    // close
    emit pFindBar->CloseRequested();
    QCoreApplication::processEvents();
    Check(!pFindBar->isVisible(), "find bar hides on close");

    // ---- debug-button gating: only usable after a clean compile ----
    Check(wnd.GetRunActionForTest()->isEnabled(),
          "gating: run enabled after successful compile");
    // a source file with a syntax error disables the debug buttons
    wnd.SetSourceForTest("MAIN    START\n"
                         "        BOGUS GR0\n"
                         "        END\n");
    wnd.TriggerAssembleForTest();
    QCoreApplication::processEvents();
    Check(!wnd.GetRunActionForTest()->isEnabled(),
          "gating: run disabled after syntax error");
    Check(!wnd.GetStepActionForTest()->isEnabled(),
          "gating: step disabled after syntax error");
    // recompiling a valid program re-enables them
    wnd.SetSourceForTest("MAIN    START\n"
                         "        EXIT\n"
                         "        END\n");
    wnd.TriggerAssembleForTest();
    Check(Pump([&] { return wnd.GetRunnerForTest()->GetSnapshot().m_eState ==
                           ERunState::stReady; },
               3000),
          "valid program recompiled and loaded");
    Check(wnd.GetRunActionForTest()->isEnabled(),
          "gating: run re-enabled after clean recompile");

    // ---- watch panel: live values + runtime editing (VC6 style) ----
    wnd.SetSourceForTest(
        "MAIN    START\n"
        "        OUT   MSG,LNG\n"
        "        EXIT\n"
        "MSG     DC    'HELLO, CASL!'\n"
        "LNG     DC    13\n"
        "        END\n");
    wnd.TriggerAssembleForTest();
    Check(Pump([&] { return wnd.GetRunnerForTest()->GetSnapshot().m_eState ==
                           ERunState::stReady; },
               3000),
          "watch program compiled and loaded");
    CSymbolsPanel* pSymbols = wnd.GetSymbolsPanelForTest();
    QCoreApplication::processEvents();
    // find the MSG row: column 0 = name
    int nMsgRow = -1, nLngRow = -1;
    for (int i = 0; i < pSymbols->GetRowCountForTest(); ++i) {
        if (pSymbols->GetCellTextForTest(i, 0) == "MSG") nMsgRow = i;
        if (pSymbols->GetCellTextForTest(i, 0) == "LNG") nLngRow = i;
    }
    Check(nMsgRow >= 0 && nLngRow >= 0, "watch: MSG and LNG rows exist");
    Check(pSymbols->GetCellTextForTest(nMsgRow, 2).startsWith("0048"),
          "watch: MSG shows first char 'H' = 0048");
    Check(pSymbols->GetCellTextForTest(nLngRow, 2).startsWith("000D"),
          "watch: LNG shows 13 = 000D");
    // content column decodes the whole DC '...' string
    Check(pSymbols->GetCellTextForTest(nMsgRow, 3) == "'HELLO, CASL!'",
          "watch: MSG content column shows the whole string");
    Check(pSymbols->GetCellTextForTest(nLngRow, 3) == "13",
          "watch: LNG content column shows decimal 13");
    // runtime edit via the watch panel: MSG <- 0051 ('Q')
    pSymbols->EditValueForTest(nMsgRow, "0051");
    Check(Pump([&] {
              std::vector<uint16_t> arrWords;
              wnd.GetRunnerForTest()->CopyMemory(arrWords);
              // MSG address from the watch row's address column
              bool bOk = false;
              int nAddr = pSymbols->GetCellTextForTest(nMsgRow, 1)
                              .toInt(&bOk, 16);
              return bOk && arrWords[(size_t)nAddr] == 0x0051;
          },
          3000),
          "watch: editing MSG to 0051 patches machine memory");
    // content column follows the runtime change (0051 = 'Q', rest unchanged)
    QCoreApplication::processEvents();
    Check(pSymbols->GetCellTextForTest(nMsgRow, 3) == "'QELLO, CASL!'",
          "watch: content column updates after edit");
    // whole-string edit: 'XYZ' writes 3 consecutive words
    pSymbols->EditValueForTest(nMsgRow, "'XYZ'");
    Check(Pump([&] {
              std::vector<uint16_t> arrWords;
              wnd.GetRunnerForTest()->CopyMemory(arrWords);
              bool bOk = false;
              int nAddr = pSymbols->GetCellTextForTest(nMsgRow, 1)
                              .toInt(&bOk, 16);
              return bOk && arrWords[(size_t)nAddr] == 0x0058 &&
                     arrWords[(size_t)nAddr + 1] == 0x0059 &&
                     arrWords[(size_t)nAddr + 2] == 0x005A;
          },
          3000),
          "watch: editing MSG to 'XYZ' patches 3 consecutive words");
    // content-column editing: unquoted text on a string row = raw content
    pSymbols->EditContentForTest(nMsgRow, "NI HAO");
    Check(Pump([&] {
              std::vector<uint16_t> arrWords;
              wnd.GetRunnerForTest()->CopyMemory(arrWords);
              bool bOk = false;
              int nAddr = pSymbols->GetCellTextForTest(nMsgRow, 1)
                              .toInt(&bOk, 16);
              return bOk && arrWords[(size_t)nAddr] == 0x004E &&
                     arrWords[(size_t)nAddr + 1] == 0x0049 &&
                     arrWords[(size_t)nAddr + 2] == 0x0020 &&
                     arrWords[(size_t)nAddr + 3] == 0x0048 &&
                     arrWords[(size_t)nAddr + 4] == 0x0041 &&
                     arrWords[(size_t)nAddr + 5] == 0x004F;
          },
          3000),
          "content edit: 'NI HAO' written as 6 consecutive words");
    // content-column editing on a numeric row: plain decimal
    pSymbols->EditContentForTest(nLngRow, "20");
    Check(Pump([&] {
              std::vector<uint16_t> arrWords;
              wnd.GetRunnerForTest()->CopyMemory(arrWords);
              bool bOk = false;
              int nAddr = pSymbols->GetCellTextForTest(nLngRow, 1)
                              .toInt(&bOk, 16);
              return bOk && arrWords[(size_t)nAddr] == 20;
          },
          3000),
          "content edit: LNG -> 20 (single decimal word)");
    // decimal and negative parsing
    uint16_t w = 0;
    Check(CMainWindow::ParseWordTextForTest("65", w) && w == 0x0041,
          "parse: decimal 65 -> 0041");
    Check(CMainWindow::ParseWordTextForTest("-1", w) && w == 0xFFFF,
          "parse: decimal -1 -> FFFF");
    Check(CMainWindow::ParseWordTextForTest("#FF", w) && w == 0x00FF,
          "parse: #FF -> 00FF");
    Check(CMainWindow::ParseWordTextForTest("0041", w) && w == 0x0041,
          "parse: hex 0041 -> 0041");
    Check(CMainWindow::ParseWordTextForTest("'A'", w) && w == 0x0041,
          "parse: 'A' -> 0041");
    Check(CMainWindow::ParseWordTextForTest("'AB'", w) && w == 0x0041,
          "parse: 'AB' takes first char -> 0041");
    Check(!CMainWindow::ParseWordTextForTest("xyz", w),
          "parse: junk rejected");

    std::printf("\n%d passed, %d failed\n", s_nPassed, s_nFailed);
    return s_nFailed == 0 ? 0 : 1;
}
