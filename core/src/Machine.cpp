#include "casl/Machine.hpp"

#include <algorithm>

namespace casl {

CMachine::CMachine() {
    std::fill(m_arrMemory.begin(), m_arrMemory.end(), int16_t(0));
}

void CMachine::Load(const std::vector<uint16_t>& arrWords, int nStartAddress) {
    m_arrImage = arrWords;
    std::fill(m_arrMemory.begin(), m_arrMemory.end(), int16_t(0));
    for (size_t i = 0; i < arrWords.size() && i < (size_t)MEMORY_WORDS; ++i)
        m_arrMemory[i] = (int16_t)arrWords[i];
    m_nStartAddress = nStartAddress & 0xFFFF;
    Reset();
}

void CMachine::Reset() {
    std::fill(m_arrGr.begin(), m_arrGr.end(), int16_t(0));
    m_wSp = INITIAL_SP;
    m_wPr = (uint16_t)m_nStartAddress;
    m_bOf = m_bSf = m_bZf = false;
    m_nSteps = 0;
    m_strLastError.clear();
    m_eState = ERunState::stReady;
}

CMachineState CMachine::Snapshot() const {
    CMachineState st;
    st.m_eState = m_eState;
    st.m_arrGr = m_arrGr;
    st.m_wSp = m_wSp;
    st.m_wPr = m_wPr;
    st.m_bOf = m_bOf;
    st.m_bSf = m_bSf;
    st.m_bZf = m_bZf;
    st.m_nSteps = m_nSteps;
    st.m_strLastError = m_strLastError;
    return st;
}

bool CMachine::GetFlag(EFlag eFlag) const {
    switch (eFlag) {
    case EFlag::flOF: return m_bOf;
    case EFlag::flSF: return m_bSf;
    case EFlag::flZF: return m_bZf;
    }
    return false;
}

bool CMachine::CanStep() const {
    return m_eState == ERunState::stReady || m_eState == ERunState::stRunning ||
           m_eState == ERunState::stPaused;
}

void CMachine::SetLogicFlags(uint16_t wResult, bool bCarry) {
    m_bOf = bCarry;
    m_bSf = (wResult & 0x8000) != 0;
    m_bZf = wResult == 0;
}

void CMachine::Halt(ERunState eFinalState, const std::string& strMessage) {
    m_eState = eFinalState;
    m_strLastError = strMessage;
    if (m_fnHaltHook) m_fnHaltHook(m_eState, strMessage);
}

void CMachine::Push(uint16_t wValue) {
    m_wSp = (uint16_t)(m_wSp - 1);
    WriteWord(m_wSp, wValue);
}

uint16_t CMachine::Pop() {
    uint16_t wValue = ReadWord(m_wSp);
    m_wSp = (uint16_t)(m_wSp + 1);
    return wValue;
}

uint16_t CMachine::EffectiveAddr(const CDecoded& dec) const {
    uint16_t wBase = dec.m_wAddr;
    if (dec.m_byXr != 0) wBase = (uint16_t)(wBase + (uint16_t)m_arrGr[dec.m_byXr & 7]);
    return wBase;
}

CMachine::CDecoded CMachine::Fetch() {
    CDecoded dec;
    uint16_t w = ReadWord(m_wPr);
    dec.m_eOp = (EOp)((w >> 8) & 0xFF);
    dec.m_byGr = (uint8_t)((w >> 4) & 0xF);
    dec.m_byXr = (uint8_t)(w & 0xF);
    if (AddrModeOf(dec.m_eOp) == EAddrMode::amImm) {
        // SVC: function code in bits 7..0; LDR: xr already holds src register.
        dec.m_wAddr = w & 0xFF;
    } else if (AddrModeOf(dec.m_eOp) != EAddrMode::amNone) {
        dec.m_wAddr = ReadWord((uint16_t)(m_wPr + 1));
    } else {
        dec.m_wAddr = 0;
    }
    return dec;
}

int CMachine::AddressForLine(int nSrcLine) const {
    for (const auto& entry : m_arrEntries)
        if (!entry.m_bIsData && entry.m_nSrcLine == nSrcLine) return entry.m_nAddress;
    return -1;
}

bool CMachine::Step() {
    if (!CanStep()) return false;
    if (m_fnStepHook && !m_fnStepHook(m_wPr)) return false; // host wants pause/stop

    if (m_eState != ERunState::stRunning) m_eState = ERunState::stRunning;

    CDecoded dec = Fetch();
    uint16_t wNextPr = (uint16_t)(m_wPr + 1);
    if (AddrModeOf(dec.m_eOp) == EAddrMode::amRegMem ||
        AddrModeOf(dec.m_eOp) == EAddrMode::amMem)
        wNextPr = (uint16_t)(wNextPr + 1);

    auto wEa = [&]() { return EffectiveAddr(dec); };
    auto DoJump = [&]() { wNextPr = wEa(); };

    switch (dec.m_eOp) {
    case EOp::opNOP:
        break;

    case EOp::opLD: {
        uint16_t wValue = ReadWord(wEa());
        m_arrGr[dec.m_byGr] = (int16_t)wValue;
        SetLogicFlags(wValue);
        break;
    }
    case EOp::opLDR: { // register-to-register LD
        m_arrGr[dec.m_byGr] = m_arrGr[dec.m_byXr & 7];
        SetLogicFlags((uint16_t)m_arrGr[dec.m_byGr]);
        break;
    }
    case EOp::opST:
        WriteWord(wEa(), (uint16_t)m_arrGr[dec.m_byGr]);
        break;

    case EOp::opADDA:
    case EOp::opADDL:
    case EOp::opSUBA:
    case EOp::opSUBL: {
        uint16_t wA = (uint16_t)m_arrGr[dec.m_byGr];
        uint16_t wB = ReadWord(wEa());
        bool bIsAdd = (dec.m_eOp == EOp::opADDA || dec.m_eOp == EOp::opADDL);
        uint16_t wR = bIsAdd ? (uint16_t)(wA + wB) : (uint16_t)(wA - wB);
        bool bCarry;
        if (dec.m_eOp == EOp::opADDA || dec.m_eOp == EOp::opSUBA) {
            int32_t lSA = (int16_t)wA, lSB = (int16_t)wB;
            int32_t lSR = bIsAdd ? lSA + lSB : lSA - lSB;
            bCarry = lSR > 32767 || lSR < -32768;
        } else {
            uint32_t ulA = wA, ulB = wB;
            uint32_t ulR = bIsAdd ? ulA + ulB : ulA - ulB;
            bCarry = (ulR & 0xFFFF0000u) != 0;
        }
        m_arrGr[dec.m_byGr] = (int16_t)wR;
        SetLogicFlags(wR, bCarry);
        break;
    }

    case EOp::opAND:
    case EOp::opOR:
    case EOp::opXOR: {
        uint16_t wA = (uint16_t)m_arrGr[dec.m_byGr];
        uint16_t wB = ReadWord(wEa());
        uint16_t wR = dec.m_eOp == EOp::opAND ? (uint16_t)(wA & wB)
                   : dec.m_eOp == EOp::opOR  ? (uint16_t)(wA | wB)
                                             : (uint16_t)(wA ^ wB);
        m_arrGr[dec.m_byGr] = (int16_t)wR;
        SetLogicFlags(wR);
        break;
    }

    case EOp::opCPA:
    case EOp::opCPL: {
        uint16_t wA = (uint16_t)m_arrGr[dec.m_byGr];
        uint16_t wB = ReadWord(wEa());
        m_bZf = (wA == wB);
        if (dec.m_eOp == EOp::opCPA) {
            int32_t lSA = (int16_t)wA, lSB = (int16_t)wB;
            m_bSf = lSA < lSB;
            int32_t lDiff = lSA - lSB;
            m_bOf = lDiff > 32767 || lDiff < -32768;
        } else {
            m_bSf = wA < wB;
            m_bOf = false;
        }
        break;
    }

    case EOp::opSLA:
    case EOp::opSLL:
    case EOp::opSRA:
    case EOp::opSRL: {
        uint16_t wA = (uint16_t)m_arrGr[dec.m_byGr];
        // shift amount: value stored at the effective address, capped to 16
        int nShift = (int)(ReadWord(wEa()) & 0xFF);
        if (nShift > 16) nShift = 16;
        uint16_t wSign = wA & 0x8000;
        uint16_t wR = wA;
        if (nShift > 0) {
            if (dec.m_eOp == EOp::opSLA) {
                wR = (uint16_t)((wA << nShift) & 0x7FFF) | wSign;
            } else if (dec.m_eOp == EOp::opSRA) {
                uint16_t wBits = (uint16_t)(wA >> nShift);
                if (wSign) wBits |= (uint16_t)(0xFFFF << (16 - nShift));
                wR = wBits;
            } else if (dec.m_eOp == EOp::opSLL) {
                wR = (uint16_t)(wA << nShift);
            } else {
                wR = (uint16_t)(wA >> nShift);
            }
        }
        m_arrGr[dec.m_byGr] = (int16_t)wR;
        SetLogicFlags(wR);
        break;
    }

    case EOp::opJUMP: DoJump(); break;
    case EOp::opJPL:  if (!m_bSf && !m_bZf) DoJump(); break;
    case EOp::opJMI:  if (m_bSf) DoJump(); break;
    case EOp::opJNZ:  if (!m_bZf) DoJump(); break;
    case EOp::opJZE:  if (m_bZf) DoJump(); break;
    case EOp::opJOV:  if (m_bOf) DoJump(); break;

    case EOp::opPUSH: Push(ReadWord(wEa())); break;
    case EOp::opPOP:  m_arrGr[dec.m_byGr] = (int16_t)Pop(); break;

    case EOp::opCALL:
        Push((uint16_t)(m_wPr + 2)); // return address (after operand word)
        DoJump();
        break;

    case EOp::opRET:
        wNextPr = Pop();
        break;

    case EOp::opSVC: {
        uint16_t wCode = dec.m_wAddr;
        if (wCode == (uint16_t)ESvcCode::svcEXIT) {
            Halt(ERunState::stHalted);
            return false;
        }
        if (wCode == (uint16_t)ESvcCode::svcIN || wCode == (uint16_t)ESvcCode::svcOUT) {
            // The assembler placed (bufferAddr, lenLabelAddr) after the SVC word.
            uint16_t wBufAddr = ReadWord((uint16_t)(m_wPr + 1));
            uint16_t wLenAddr = ReadWord((uint16_t)(m_wPr + 2));
            wNextPr = (uint16_t)(m_wPr + 3);
            if (wCode == (uint16_t)ESvcCode::svcOUT) {
                int nLen = ReadWord(wLenAddr);
                if (nLen < 0) nLen = 0;
                if (nLen > 256) nLen = 256;
                std::string strText;
                for (int i = 0; i < nLen; ++i)
                    strText += (char)(ReadWord((uint16_t)(wBufAddr + i)) & 0xFF);
                if (m_fnOutputHook) m_fnOutputHook(strText);
            } else {
                if (m_fnInputHook) {
                    std::string strLine = m_fnInputHook();
                    int nLen = (int)strLine.size();
                    if (nLen > 256) nLen = 256;
                    for (int i = 0; i < nLen; ++i)
                        WriteWord((uint16_t)(wBufAddr + i),
                                  (uint16_t)(unsigned char)strLine[i]);
                    WriteWord(wLenAddr, (uint16_t)nLen);
                } else {
                    WriteWord(wLenAddr, 0);
                }
            }
            break;
        }
        Halt(ERunState::stError, "unknown SVC code " + std::to_string(wCode));
        return false;
    }

    default:
        Halt(ERunState::stError,
             "illegal opcode 0x" + std::to_string((unsigned)dec.m_eOp) + " at 0x" +
                 std::to_string(m_wPr));
        return false;
    }

    m_wPr = wNextPr;
    ++m_nSteps;
    return true;
}

} // namespace casl
