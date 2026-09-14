// Headless regression test for CMachineRunner single-step debugging.
//
// Reproduces the reported bug: after a run pauses at a breakpoint, F10
// (StepOnce) must execute exactly one instruction, and the published state
// must be stPaused (not stRunning) so the UI keeps the debug actions enabled.
//
// Exit code 0 = all checks passed.

#include "MachineRunner.hpp"
#include "casl/Assembler.hpp"

#include <QElapsedTimer>
#include <QCoreApplication>
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

// Pump the event loop until pred() holds or the timeout elapses.
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

const char* s_pszSrc =
    "MAIN    START\n"
    "        LD    GR0,#0\n"
    "LOOP    ADDA  GR0,#1\n"
    "        CPA   GR0,#3\n"
    "        JMI   LOOP\n"
    "        EXIT\n"
    "        END\n";

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    casl::CAssembleResult result = casl::Assemble(s_pszSrc);
    Check(result.m_bOk, "assemble test program");
    if (!result.m_bOk) return 1;

    // address of the ADDA (source line 3) = 2: LD occupies words 0-1
    int nBpAddr = -1;
    for (const auto& pairLA : result.m_arrLineMap)
        if (pairLA.first == 3) { nBpAddr = pairLA.second; break; }
    Check(nBpAddr == 2, "breakpoint address is word 2");

    CMachineRunner runner;
    runner.LoadProgram(result);
    Check(Pump([&] { return runner.GetSnapshot().m_eState == ERunState::stReady; },
               2000),
          "load -> stReady published");

    runner.SetBreakpoints(QSet<int>{nBpAddr});
    runner.StartRun();

    // Bug A regression: pause at the breakpoint must publish stPaused (the old
    // code left the machine in stRunning and the UI kept everything disabled)
    Check(Pump([&] { return runner.GetSnapshot().m_eState == ERunState::stPaused; },
               2000),
          "breakpoint hit -> stPaused (UI re-enables)");
    Check(runner.GetSnapshot().m_wPr == (uint16_t)nBpAddr,
          "paused exactly at the breakpoint");
    Check(runner.GetSnapshot().m_nSteps == 1, "LD executed before the pause");

    // Bug B regression: stepping from the breakpoint must execute one
    // instruction per press (the old code refused forever at the same PR)
    const int arrExpectedPr[4] = {4, 6, 2, 4}; // ADDA, CPA, JMI, ADDA again
    int nStepsBefore = runner.GetSnapshot().m_nSteps;
    bool bAllStepsOk = true;
    for (int i = 0; i < 4; ++i) {
        runner.StepOnce();
        const int nTargetSteps = nStepsBefore + i + 1;
        const bool bThisStep = Pump([&] {
            return runner.GetSnapshot().m_nSteps == nTargetSteps;
        }, 2000);
        const uint16_t wPr = runner.GetSnapshot().m_wPr;
        const ERunState eState = runner.GetSnapshot().m_eState;
        if (!bThisStep || wPr != (uint16_t)arrExpectedPr[i] ||
            eState != ERunState::stPaused) {
            bAllStepsOk = false;
            std::printf(
                "  step %d: steps=%d pr=0x%X state=%d (want steps=%d pr=0x%X "
                "state=stPaused)\n",
                i + 1, runner.GetSnapshot().m_nSteps, (int)wPr, (int)eState,
                nTargetSteps, arrExpectedPr[i]);
        }
    }
    Check(bAllStepsOk, "4x F10 from breakpoint: one instruction each, stays stPaused");

    // clear breakpoints and run to completion
    runner.SetBreakpoints(QSet<int>{});
    runner.StartRun();
    Check(Pump([&] { return runner.GetSnapshot().m_eState == ERunState::stHalted; },
               2000),
          "continue without breakpoints -> stHalted");
    Check(runner.GetSnapshot().m_arrGr[0] == 3, "GR0 == 3 at exit");

    std::printf("\n%d passed, %d failed\n", s_nPassed, s_nFailed);
    return s_nFailed == 0 ? 0 : 1;
}
