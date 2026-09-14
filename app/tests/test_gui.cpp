// Offscreen GUI test: drives the real CMainWindow (actions, docks, panels,
// runner connections, event loop) through assemble + single-step and reads the
// register panel labels directly. Catches UI-side breakage that test_runner
// (runner-only) cannot see. Run with QT_QPA_PLATFORM=offscreen.
//
// Exit code 0 = all checks passed.

#include "MainWindow.hpp"
#include "MachineRunner.hpp"
#include "Panels.hpp"

#include "casl/Machine.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QTimer>
#include <QThread>

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

    std::printf("\n%d passed, %d failed\n", s_nPassed, s_nFailed);
    return s_nFailed == 0 ? 0 : 1;
}
