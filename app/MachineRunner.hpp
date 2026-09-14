// Machine execution worker: runs the CMachine on a dedicated std::thread with
// run / pause / step / stop / breakpoint / IO bridging to the UI.
// The UI thread never touches the machine directly: it posts commands and
// reads mutex-guarded snapshots.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include "casl/Assembler.hpp"
#include "casl/Machine.hpp"

#include <QByteArray>
#include <QObject>
#include <QSet>
#include <QString>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class CMachineRunner : public QObject {
    Q_OBJECT

public:
    explicit CMachineRunner(QObject* pParent = nullptr);
    ~CMachineRunner() override;

    // Queue a new program image (thread-safe, returns immediately).
    void LoadProgram(const casl::CAssembleResult& result);
    bool HasProgram() const { return m_bLoaded; }

    // UI-thread inspection: read cached snapshots (internally locked).
    casl::CMachineState GetSnapshot() const;
    void CopyMemory(std::vector<uint16_t>& arrWords) const;
    // runtime memory patch (watch/memory edit): writes one word into the
    // live machine (if loaded) and the published snapshot, then forces a
    // StateChanged publish so panels refresh immediately
    void WriteMemoryWord(int nAddress, uint16_t wValue);
    // writes a run of consecutive words (e.g. a whole string constant)
    void WriteMemoryWords(int nAddress, const std::vector<uint16_t>& arrValues);

public slots:
    void StartRun();                    // resume / start continuous run
    void StartRunNoDebug();             // continuous run, breakpoints ignored
    void StartStepOut();                // run until SP grows (subroutine returns)
    void StepOnce();                    // execute exactly one instruction
    void RequestStop();                 // stop the run loop, keep state
    void ResetMachine();                // reinitialize the loaded program
    void ProvideInput(const QString& strLine);
    void SetBreakpoints(const QSet<int>& setAddresses);

signals:
    void StateChanged(const casl::CMachineState& state);
    void OutputReady(const QString& strText);
    void InputNeeded();
    // Run loop ended: bClean true = clean halt, strError set on failure.
    void RunFinished(bool bClean, const QString& strError);
    // Queued when a LoadProgram command has been applied (assembled OK).
    void ProgramLoaded(bool bOk);

private:
    enum ECommand {
        cmdNone = 0,
        cmdLoad = 1,
        cmdReset = 2,
        cmdRun = 3,
        cmdStep = 4,
        cmdStop = 5,
        cmdQuit = 6,
    };

    void WorkerLoop();
    bool CheckStepHook(uint16_t wPr);
    // requires m_mtx held; throttled unless bForce (pause/step/finish)
    void PublishStateLocked(bool bForce = true);
    void ApplyLoadLocked();    // requires m_mtx held

    // ---- machine: owned by the worker thread ----------------------------
    std::unique_ptr<casl::CMachine> m_pMachine;

    // ---- shared state (guarded by m_mtx) --------------------------------
    mutable std::mutex m_mtx;
    std::condition_variable m_cv;
    ECommand m_eCommand = cmdNone;
    bool m_bLoaded = false;
    bool m_bRunning = false;      // worker is inside the run loop
    bool m_bStepPending = false;
    bool m_bStopRequested = false;
    bool m_bIgnoreBreakOnce = false;
    QSet<int> m_setBreakAddresses;
    bool m_bWaitingInput = false;
    QString m_strPendingInput;
    bool m_bIgnoreBreakpoints = false;  // "run without debugging" mode
    bool m_bStepOutActive = false;      // run until subroutine returns
    uint16_t m_wStepOutSp = 0;
    casl::CAssembleResult m_resultPending;
    casl::CMachineState m_stateSnapshot;
    std::vector<uint16_t> m_arrMemSnapshot;
    std::chrono::steady_clock::time_point m_tLastPublish{};

    std::thread m_thread;
};
