// COMET II virtual machine with debugging hooks. Pure C++17, no Qt.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include "casl/OpCode.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace casl {

enum class ERunState {
    stIdle,         // no program loaded
    stReady,        // loaded, not started
    stRunning,      // executing
    stWaitingInput, // blocked in an SVC IN
    stPaused,       // stopped at breakpoint / after single step
    stHalted,       // EXIT executed or stopped
    stError,
};

class CMachineState {
public:
    ERunState                        m_eState = ERunState::stIdle;
    std::array<int16_t, REGISTER_COUNT> m_arrGr{};
    uint16_t                         m_wSp = 0;
    uint16_t                         m_wPr = 0;
    bool                             m_bOf = false, m_bSf = false, m_bZf = false;
    int                              m_nSteps = 0;
    std::string                      m_strLastError;
};

// A COMET II machine. Not thread-safe by itself: the host (UI worker thread)
// must serialize access. Hooks are provided for IO, stepping and breakpoints.
class CMachine {
public:
    CMachine();

    // ---- program loading -------------------------------------------------
    void Load(const std::vector<uint16_t>& arrWords, int nStartAddress = 0);
    void Reset(); // back to state right after Load()

    // ---- execution control ------------------------------------------------
    // Executes exactly one instruction. Returns false when the machine is in
    // a terminal state (Halted/Error) or the step hook requested a stop.
    bool Step();
    bool CanStep() const;

    ERunState GetState() const { return m_eState; }
    CMachineState Snapshot() const;
    void SetStateForHost(ERunState eState) { m_eState = eState; }

    // ---- hooks -------------------------------------------------------------
    void SetStepHook(std::function<bool(uint16_t wPr)> fn) { m_fnStepHook = std::move(fn); }
    void SetOutputHook(std::function<void(const std::string&)> fn) { m_fnOutputHook = std::move(fn); }
    void SetInputHook(std::function<std::string()> fn) { m_fnInputHook = std::move(fn); }
    void SetHaltHook(std::function<void(ERunState, const std::string&)> fn) { m_fnHaltHook = std::move(fn); }

    // ---- inspection --------------------------------------------------------
    int16_t  Gr(int nIndex) const { return m_arrGr[nIndex & 7]; }
    uint16_t Sp() const { return m_wSp; }
    uint16_t Pr() const { return m_wPr; }
    bool     GetFlag(EFlag eFlag) const;
    int16_t  Mem(int nAddr) const { return m_arrMemory[nAddr & 0xFFFF]; }
    void     SetMem(int nAddr, int16_t nValue) { m_arrMemory[nAddr & 0xFFFF] = nValue; }
    int      Steps() const { return m_nSteps; }
    const std::string& GetLastError() const { return m_strLastError; }
    const std::vector<CCodeEntry>& GetEntries() const { return m_arrEntries; }
    void SetEntries(std::vector<CCodeEntry> arrEntries) { m_arrEntries = std::move(arrEntries); }
    // Address (first word) of the instruction that the given source line
    // produced, or -1. Used to map source-line breakpoints.
    int AddressForLine(int nSrcLine) const;

private:
    struct CDecoded {
        EOp      m_eOp = EOp::opNOP;
        uint8_t  m_byGr = 0;
        uint8_t  m_byXr = 0;
        uint16_t m_wAddr = 0;
    };

    CDecoded   Fetch();
    uint16_t   EffectiveAddr(const CDecoded& dec) const;
    uint16_t   ReadWord(uint16_t wAddr) const { return (uint16_t)m_arrMemory[wAddr & 0xFFFF]; }
    void       WriteWord(uint16_t wAddr, uint16_t wValue) { m_arrMemory[wAddr & 0xFFFF] = (int16_t)wValue; }
    void       Push(uint16_t wValue);
    uint16_t   Pop();
    void       SetLogicFlags(uint16_t wResult, bool bCarry = false);
    void       Halt(ERunState eFinalState, const std::string& strMessage = "");

    std::array<int16_t, MEMORY_WORDS>     m_arrMemory{};
    std::array<int16_t, REGISTER_COUNT>   m_arrGr{};
    uint16_t                              m_wSp = 0;
    uint16_t                              m_wPr = 0;
    bool                                  m_bOf = false, m_bSf = false, m_bZf = false;
    ERunState                             m_eState = ERunState::stIdle;
    int                                   m_nStartAddress = 0;
    std::vector<uint16_t>                 m_arrImage;
    int                                   m_nSteps = 0;
    std::string                           m_strLastError;
    std::vector<CCodeEntry>               m_arrEntries;

    std::function<bool(uint16_t)>                       m_fnStepHook;
    std::function<void(const std::string&)>             m_fnOutputHook;
    std::function<std::string()>                        m_fnInputHook;
    std::function<void(ERunState, const std::string&)>  m_fnHaltHook;
};

} // namespace casl
