#include "MachineRunner.hpp"

#include <chrono>

Q_DECLARE_METATYPE(casl::CMachineState)

CMachineRunner::CMachineRunner(QObject* pParent) : QObject(pParent) {
    qRegisterMetaType<casl::CMachineState>("casl::CMachineState");
    m_pMachine = std::make_unique<casl::CMachine>();
    m_arrMemSnapshot.assign(casl::MEMORY_WORDS, 0);

    m_pMachine->SetOutputHook([this](const std::string& strText) {
        emit OutputReady(QString::fromStdString(strText));
    });
    m_pMachine->SetInputHook([this]() -> std::string {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_bWaitingInput = true;
        }
        m_cv.notify_all();
        emit InputNeeded();
        std::unique_lock<std::mutex> lock(m_mtx);
        while (m_bWaitingInput && !m_bStopRequested)
            m_cv.wait_for(lock, std::chrono::milliseconds(50));
        std::string strLine = m_strPendingInput.toStdString();
        m_bWaitingInput = false;
        m_strPendingInput.clear();
        return strLine;
    });
    m_pMachine->SetStepHook(
        [this](uint16_t wPr) { return CheckStepHook(wPr); });

    m_thread = std::thread(&CMachineRunner::WorkerLoop, this);
}

CMachineRunner::~CMachineRunner() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_eCommand = cmdQuit;
        m_bStopRequested = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void CMachineRunner::LoadProgram(const casl::CAssembleResult& result) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_resultPending = result;
        m_eCommand = cmdLoad;
        m_bStopRequested = true; // interrupt any active run
    }
    m_cv.notify_all();
}

void CMachineRunner::StartRun() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (!m_bLoaded) return;
        m_bStopRequested = false;
        m_bRunning = true;
        m_bIgnoreBreakpoints = false;
        m_bStepOutActive = false;
        m_bIgnoreBreakOnce = true; // no instant break on the resume PC
    }
    m_cv.notify_all();
}

void CMachineRunner::StartRunNoDebug() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (!m_bLoaded) return;
        m_bStopRequested = false;
        m_bRunning = true;
        m_bIgnoreBreakpoints = true;
        m_bStepOutActive = false;
    }
    m_cv.notify_all();
}

void CMachineRunner::StartStepOut() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (!m_bLoaded) return;
        m_bStopRequested = false;
        m_bRunning = true;
        m_bIgnoreBreakpoints = false;
        m_bStepOutActive = true;
        m_wStepOutSp = m_stateSnapshot.m_wSp; // pause once the stack shrinks
    }
    m_cv.notify_all();
}

void CMachineRunner::StepOnce() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (!m_bLoaded) return;
        m_bStopRequested = false;
        m_bStepPending = true;
        // The step hook fires BEFORE the instruction with the current PR. If
        // we are standing on a breakpoint, it would refuse to execute (that
        // is what paused us here) and F10 would appear dead. Ignore the
        // breakpoint at the current PC exactly once, like VS single-stepping.
        m_bIgnoreBreakOnce = true;
    }
    m_cv.notify_all();
}

void CMachineRunner::RequestStop() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_bStopRequested = true;
    }
    m_cv.notify_all();
}

void CMachineRunner::ResetMachine() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (!m_bLoaded) return;
        m_eCommand = cmdReset;
        m_bStopRequested = true; // interrupt any active run
    }
    m_cv.notify_all();
}

void CMachineRunner::ProvideInput(const QString& strLine) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_strPendingInput = strLine;
        m_bWaitingInput = false;
    }
    m_cv.notify_all();
}

void CMachineRunner::SetBreakpoints(const QSet<int>& setAddresses) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_setBreakAddresses = setAddresses;
}

casl::CMachineState CMachineRunner::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_stateSnapshot;
}

void CMachineRunner::CopyMemory(std::vector<uint16_t>& arrWords) const {
    std::lock_guard<std::mutex> lock(m_mtx);
    arrWords = m_arrMemSnapshot;
}

bool CMachineRunner::CheckStepHook(uint16_t wPr) {
    // called from the machine (worker thread) before each instruction
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_bStopRequested) return false;
    if (m_bWaitingInput) return true; // do not treat input waits as pauses
    if (m_bStepOutActive) {
        // "step out": stop as soon as the stack pointer rises above the
        // captured baseline (RET/POP of the current frame) or we halt
        if ((uint16_t)m_pMachine->Sp() > m_wStepOutSp) {
            m_bStepOutActive = false;
            return false;
        }
        return true;
    }
    if (!m_bIgnoreBreakpoints && m_setBreakAddresses.contains((int)wPr)) {
        if (m_bIgnoreBreakOnce) {
            m_bIgnoreBreakOnce = false;
            return true;
        }
        return false; // RunLoop notices the false return and pauses
    }
    m_bIgnoreBreakOnce = false;
    return true;
}

void CMachineRunner::PublishStateLocked(bool bForce) {
    // throttle during continuous runs so the UI event loop is not flooded
    auto tNow = std::chrono::steady_clock::now();
    if (!bForce &&
        std::chrono::duration_cast<std::chrono::milliseconds>(tNow - m_tLastPublish)
                .count() < 30)
        return;
    m_tLastPublish = tNow;

    m_stateSnapshot = m_pMachine->Snapshot();
    for (int i = 0; i < casl::MEMORY_WORDS; ++i)
        m_arrMemSnapshot[i] = (uint16_t)m_pMachine->Mem(i);
    casl::CMachineState stateCopy = m_stateSnapshot;
    m_mtx.unlock();
    emit StateChanged(stateCopy);
    m_mtx.lock();
}

void CMachineRunner::ApplyLoadLocked() {
    m_pMachine->Load(m_resultPending.m_arrWords, m_resultPending.m_nStartAddress);
    m_pMachine->SetEntries(m_resultPending.m_arrEntries);
    m_bLoaded = true;
    m_bStopRequested = false;
    PublishStateLocked();
    m_mtx.unlock();
    emit ProgramLoaded(true);
    m_mtx.lock();
}

void CMachineRunner::WorkerLoop() {
    for (;;) {
        bool bDoStep = false;
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            ECommand eCmd = m_eCommand;
            m_eCommand = cmdNone; // consume the command exactly once
            if (eCmd == cmdQuit) break;

            if (eCmd == cmdLoad) {
                ApplyLoadLocked();
                continue;
            }
            if (eCmd == cmdReset) {
                m_pMachine->Reset();
                m_bStopRequested = false;
                m_bStepPending = false;
                PublishStateLocked();
                continue;
            }
            if (eCmd == cmdStop) {
                m_bStopRequested = false;
                PublishStateLocked();
                continue;
            }

            if (!m_bLoaded || m_bStopRequested) {
                if (m_bStopRequested) {
                    m_bStopRequested = false;
                    m_bStepPending = false;
                    PublishStateLocked();
                }
                m_cv.wait_for(lock, std::chrono::milliseconds(50));
                continue;
            }
            if (m_bStepPending) {
                m_bStepPending = false;
                bDoStep = true;
            } else if (m_bRunning) {
                bDoStep = true;
            } else {
                m_cv.wait_for(lock, std::chrono::milliseconds(50));
                continue;
            }
        }

        // Execute one instruction outside the lock. The step hook may block
        // in the input hand-off; stop requests interrupt it via the cv.
        bool bStepped = m_pMachine->Step();
        casl::ERunState eState = m_pMachine->GetState();

        {
            std::unique_lock<std::mutex> lock(m_mtx);
            bool bFinished = false;
            bool bClean = true;
            QString strError;

            if (eState == casl::ERunState::stHalted ||
                eState == casl::ERunState::stError) {
                bClean = (eState == casl::ERunState::stHalted);
                if (!bClean)
                    strError = QString::fromStdString(m_pMachine->GetLastError());
                m_bRunning = false;
                bFinished = true;
            } else if (m_bStopRequested) {
                // manual stop (Shift+F5): show as paused, not "busy", so the
                // debug actions re-enable immediately
                m_pMachine->SetStateForHost(casl::ERunState::stPaused);
                m_bRunning = false;
                m_bStopRequested = false;
                bFinished = true;
            } else if (!bStepped) {
                // breakpoint hit: pause, but stay "loaded and resumable"
                m_pMachine->SetStateForHost(casl::ERunState::stPaused);
                m_bRunning = false;
            }
            // Any pause with the run flag down (single step finished,
            // breakpoint hit, manual stop) must publish stPaused, never a
            // stale stRunning: the UI treats stRunning as busy and would
            // grey out the debug actions after the very first F10.
            if (!m_bRunning &&
                m_pMachine->GetState() == casl::ERunState::stRunning)
                m_pMachine->SetStateForHost(casl::ERunState::stPaused);
            // throttle while running; force refresh on pause/finish/single step
            PublishStateLocked(!m_bRunning);
            if (bFinished) {
                m_mtx.unlock();
                emit RunFinished(bClean, strError);
                m_mtx.lock();
            }
        }
    }
}
