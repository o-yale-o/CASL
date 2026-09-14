// CASL II two-pass assembler implementation.
// Naming convention: MFC-style Hungarian notation.
#include "casl/Assembler.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>

namespace casl {

namespace {

// ---------------------------------------------------------------------------
// small string helpers
// ---------------------------------------------------------------------------

std::string Trim(const std::string& strText) {
    size_t uBeg = strText.find_first_not_of(" \t\r\n");
    if (uBeg == std::string::npos) return "";
    size_t uEnd = strText.find_last_not_of(" \t\r\n");
    return strText.substr(uBeg, uEnd - uBeg + 1);
}

std::string Upper(const std::string& strText) {
    std::string strResult = strText;
    std::transform(strResult.begin(), strResult.end(), strResult.begin(),
                   [](unsigned char ch) { return (char)std::toupper(ch); });
    return strResult;
}

std::vector<std::string> SplitComma(const std::string& strText) {
    std::vector<std::string> arrOut;
    std::string strCur;
    bool bInQuote = false;
    for (char ch : strText) {
        if (ch == '\'') bInQuote = !bInQuote;
        if (ch == ',' && !bInQuote) {
            arrOut.push_back(Trim(strCur));
            strCur.clear();
        } else {
            strCur += ch;
        }
    }
    arrOut.push_back(Trim(strCur));
    return arrOut;
}

bool IsIdentStart(char ch) { return std::isalpha((unsigned char)ch) || ch == '_'; }
bool IsIdentChar(char ch) { return std::isalnum((unsigned char)ch) || ch == '_'; }

bool IsSymbolLike(const std::string& strText) {
    if (strText.empty() || !IsIdentStart(strText[0])) return false;
    for (char ch : strText)
        if (!IsIdentChar(ch)) return false;
    return true;
}

// ---------------------------------------------------------------------------
// parsed source line
// ---------------------------------------------------------------------------

class CSourceLine {
public:
    int                      m_nNo = 0;      // 1-based source line number
    std::string              m_strLabel;     // optional label (as written)
    std::string              m_strOp;        // instruction (upper)
    std::vector<std::string> m_arrOperands;
    bool                     m_bEmpty = false;
};

std::vector<CSourceLine> ParseLines(const std::string& strSource,
                                    std::vector<CAsmError>& arrErrors) {
    std::vector<CSourceLine> arrLines;
    std::istringstream istr(strSource);
    std::string strRaw;
    int nNo = 0;
    while (std::getline(istr, strRaw)) {
        ++nNo;
        size_t uPos = strRaw.find(';');
        if (uPos != std::string::npos) strRaw = strRaw.substr(0, uPos);
        strRaw = Trim(strRaw);
        CSourceLine sl;
        sl.m_nNo = nNo;
        if (strRaw.empty()) {
            sl.m_bEmpty = true;
            arrLines.push_back(sl);
            continue;
        }
        size_t i = 0;
        if (IsIdentStart(strRaw[0])) {
            size_t j = 0;
            while (j < strRaw.size() && IsIdentChar(strRaw[j])) ++j;
            std::string strHead = strRaw.substr(0, j);
            static const std::set<std::string> s_arrOps = {
                "START", "END", "DS", "DC", "IN", "OUT", "EXIT", "NOP", "LD",
                "ST", "ADDA", "ADDL", "SUBA", "SUBL", "AND", "OR", "XOR",
                "CPA", "CPL", "SLA", "SRA", "SLL", "SRL", "JUMP", "JPL",
                "JMI", "JNZ", "JZE", "JOV", "PUSH", "POP", "CALL", "RET",
                "SVC"};
            bool bHeadIsOp = s_arrOps.count(Upper(strHead)) != 0;
            // CASL: if the first token is not an opcode, it must be a label.
            if (!bHeadIsOp) {
                sl.m_strLabel = strHead;
                i = j;
                while (i < strRaw.size() && (strRaw[i] == ' ' || strRaw[i] == '\t')) ++i;
            }
        }
        if (i >= strRaw.size() && sl.m_strLabel.empty()) {
            sl.m_bEmpty = true;
            arrLines.push_back(sl);
            continue;
        }
        if (i < strRaw.size()) {
            size_t j = i;
            while (j < strRaw.size() && IsIdentChar(strRaw[j])) ++j;
            sl.m_strOp = Upper(strRaw.substr(i, j - i));
            i = j;
            while (i < strRaw.size() && (strRaw[i] == ' ' || strRaw[i] == '\t')) ++i;
            std::string strRest = Trim(strRaw.substr(i));
            if (!strRest.empty()) sl.m_arrOperands = SplitComma(strRest);
        } else if (!sl.m_strLabel.empty()) {
            arrErrors.push_back({nNo, "missing instruction after label '" + sl.m_strLabel + "'"});
        }
        arrLines.push_back(sl);
    }
    return arrLines;
}

class CMnemoInfo {
public:
    const char* m_pszName;
    EOp         m_eOp;
};

const CMnemoInfo s_arrMnemonics[] = {
    {"NOP", EOp::opNOP},   {"LD", EOp::opLD},       {"ST", EOp::opST},
    {"ADDA", EOp::opADDA}, {"ADDL", EOp::opADDL},   {"SUBA", EOp::opSUBA},
    {"SUBL", EOp::opSUBL}, {"AND", EOp::opAND},     {"OR", EOp::opOR},
    {"XOR", EOp::opXOR},   {"CPA", EOp::opCPA},     {"CPL", EOp::opCPL},
    {"SLA", EOp::opSLA},   {"SRA", EOp::opSRA},     {"SLL", EOp::opSLL},
    {"SRL", EOp::opSRL},   {"JUMP", EOp::opJUMP},   {"JPL", EOp::opJPL},
    {"JMI", EOp::opJMI},   {"JNZ", EOp::opJNZ},     {"JZE", EOp::opJZE},
    {"JOV", EOp::opJOV},   {"PUSH", EOp::opPUSH},   {"POP", EOp::opPOP},
    {"CALL", EOp::opCALL}, {"RET", EOp::opRET},
};

bool LookupMnemonic(const std::string& strName, EOp& eOp) {
    for (const auto& mi : s_arrMnemonics)
        if (strName == mi.m_pszName) {
            eOp = mi.m_eOp;
            return true;
        }
    return false;
}

// ---------------------------------------------------------------------------
// operand evaluation
// ---------------------------------------------------------------------------

class CLiteralPool {
public:
    std::map<std::string, int> m_mapLiterals; // literal text (upper) -> address
    std::vector<std::string>   m_arrOrder;

    void Add(const std::string& strLit) {
        std::string strKey = Upper(strLit);
        if (!m_mapLiterals.count(strKey)) {
            m_mapLiterals[strKey] = -1; // assigned later
            m_arrOrder.push_back(strKey);
        }
    }
};

bool ParseSignedInt(const std::string& strText, long long& llValue) {
    std::string str = Trim(strText);
    if (str.empty()) return false;
    size_t i = 0;
    bool bNeg = false;
    if (str[0] == '+' || str[0] == '-') {
        bNeg = str[0] == '-';
        i = 1;
    }
    if (i >= str.size()) return false;
    if (str[i] == '#') {
        ++i;
        if (i >= str.size()) return false;
        long long ll = 0;
        for (; i < str.size(); ++i) {
            char ch = str[i];
            int nDigit;
            if (ch >= '0' && ch <= '9') nDigit = ch - '0';
            else if (ch >= 'A' && ch <= 'F') nDigit = ch - 'A' + 10;
            else if (ch >= 'a' && ch <= 'f') nDigit = ch - 'a' + 10;
            else return false;
            ll = ll * 16 + nDigit;
            if (ll > 0x100000) return false;
        }
        llValue = bNeg ? -ll : ll;
        return true;
    }
    long long ll = 0;
    for (; i < str.size(); ++i) {
        if (str[i] < '0' || str[i] > '9') return false;
        ll = ll * 10 + (str[i] - '0');
        if (ll > 0x100000) return false;
    }
    llValue = bNeg ? -ll : ll;
    return true;
}

// Evaluate an address expression: symbol | integer | literal | sym+n | sym-n.
// Returns false (with error pushed) when unresolvable.
bool EvalAddress(const std::string& strOperand, int nLineNo,
                 const std::map<std::string, int>& mapSymbols,
                 CLiteralPool& litPool, int& nValue,
                 std::vector<CAsmError>& arrErrors, bool bSecondPass) {
    std::string str = Trim(strOperand);
    if (str.empty()) {
        arrErrors.push_back({nLineNo, "missing address operand"});
        return false;
    }
    // A leading '#' denotes a literal constant (address of a pooled word),
    // not a direct hex address.
    if (str[0] == '#') {
        std::string strKey = Upper(str);
        long long ll;
        if (!ParseSignedInt(str, ll)) {
            arrErrors.push_back({nLineNo, "bad literal " + str});
            return false;
        }
        if (bSecondPass) {
            auto it = litPool.m_mapLiterals.find(strKey);
            if (it == litPool.m_mapLiterals.end() || it->second < 0) {
                arrErrors.push_back({nLineNo, "unresolved literal " + str});
                return false;
            }
            nValue = it->second;
            return true;
        }
        litPool.Add(str);
        nValue = 0; // placeholder
        return true;
    }
    long long llNum;
    if (ParseSignedInt(str, llNum)) {
        nValue = (int)(llNum & 0xFFFF);
        return true;
    }
    // symbol with optional +/- constant
    size_t j = 0;
    while (j < str.size() && IsIdentChar(str[j])) ++j;
    std::string strSym = Upper(str.substr(0, j));
    if (strSym.empty()) {
        arrErrors.push_back({nLineNo, "bad address operand '" + str + "'"});
        return false;
    }
    long long llOff = 0;
    if (j < str.size()) {
        if (str[j] != '+' && str[j] != '-') {
            arrErrors.push_back({nLineNo, "bad address operand '" + str + "'"});
            return false;
        }
        if (!ParseSignedInt(str.substr(j), llOff) || llOff < -32768 || llOff > 32767) {
            arrErrors.push_back({nLineNo, "bad offset in '" + str + "'"});
            return false;
        }
    }
    auto it = mapSymbols.find(strSym);
    if (it != mapSymbols.end()) {
        nValue = (int)((it->second + llOff) & 0xFFFF);
        return true;
    }
    if (bSecondPass)
        arrErrors.push_back({nLineNo, "undefined symbol '" + strSym + "'"});
    return !bSecondPass;
}

} // namespace

bool ParseIntLiteral(const std::string& strText, long long& llValue) {
    return ParseSignedInt(strText, llValue);
}

// ---------------------------------------------------------------------------
// the assembler
// ---------------------------------------------------------------------------

CAssembleResult Assemble(const std::string& strSource) {
    CAssembleResult result;
    std::vector<CAsmError>& arrErrors = result.m_arrErrors;

    std::vector<CSourceLine> arrLines = ParseLines(strSource, arrErrors);

    bool bHasStart = false, bHasEnd = false;
    for (const auto& sl : arrLines) {
        if (sl.m_strOp == "START") bHasStart = true;
        if (sl.m_strOp == "END") bHasEnd = true;
    }
    if (!bHasStart) arrErrors.push_back({0, "missing START instruction"});
    if (!bHasEnd) arrErrors.push_back({0, "missing END instruction"});

    std::map<std::string, int> mapSymbols;
    CLiteralPool litPool;
    std::string strEntryLabel;
    int nAddr = 0;

    // ---------------- pass 1: addresses & labels --------------------------
    for (const auto& sl : arrLines) {
        if (sl.m_bEmpty || sl.m_strOp.empty()) {
            if (!sl.m_strLabel.empty())
                arrErrors.push_back({sl.m_nNo, "label '" + sl.m_strLabel + "' without instruction"});
            continue;
        }
        if (!sl.m_strLabel.empty()) {
            std::string strKey = Upper(sl.m_strLabel);
            if (mapSymbols.count(strKey))
                arrErrors.push_back({sl.m_nNo, "duplicate label '" + sl.m_strLabel + "'"});
            mapSymbols[strKey] = nAddr;
        }
        if (sl.m_strOp == "START") {
            if (!sl.m_strLabel.empty()) result.m_strProgramName = sl.m_strLabel;
            if (!sl.m_arrOperands.empty()) strEntryLabel = Upper(sl.m_arrOperands[0]);
            continue;
        }
        if (sl.m_strOp == "END") continue;
        if (sl.m_strOp == "DS") {
            long long llN = 1;
            if (sl.m_arrOperands.empty() ||
                !ParseSignedInt(sl.m_arrOperands[0], llN) || llN < 0) {
                arrErrors.push_back({sl.m_nNo, "DS needs a non-negative integer"});
                llN = 1;
            }
            nAddr += (int)llN;
            continue;
        }
        if (sl.m_strOp == "DC") {
            if (sl.m_arrOperands.empty()) {
                arrErrors.push_back({sl.m_nNo, "DC needs at least one constant"});
                nAddr += 1;
                continue;
            }
            size_t uCount = 0;
            for (const auto& strConst : sl.m_arrOperands) {
                std::string str = Trim(strConst);
                if (str.empty()) continue;
                if (str[0] == '\'') {
                    bool bClosed = false;
                    for (size_t k = 1; k < str.size(); ++k) {
                        if (str[k] == '\'') {
                            if (k + 1 < str.size() && str[k + 1] == '\'') ++k;
                            else { bClosed = true; break; }
                        }
                    }
                    if (!bClosed)
                        arrErrors.push_back({sl.m_nNo, "unterminated string constant"});
                    int nChars = 0;
                    for (size_t k = 1; k < str.size();) {
                        if (str[k] == '\'' && k + 1 < str.size() && str[k + 1] == '\'') {
                            nChars += 1; k += 2;
                        } else if (str[k] == '\'') {
                            break;
                        } else {
                            nChars += 1; k += 1;
                        }
                    }
                    uCount += (size_t)std::max(nChars, 1);
                } else {
                    uCount += 1;
                }
            }
            nAddr += (int)uCount;
            continue;
        }
        if (sl.m_strOp == "IN" || sl.m_strOp == "OUT") {
            if (sl.m_arrOperands.size() != 2)
                arrErrors.push_back({sl.m_nNo, sl.m_strOp + " needs two operands: label,length"});
            nAddr += 3; // SVC word + buffer addr + length label addr
            continue;
        }
        if (sl.m_strOp == "EXIT") {
            nAddr += 1;
            continue;
        }
        if (sl.m_strOp == "SVC") {
            nAddr += 1;
            continue;
        }
        EOp eOp;
        if (LookupMnemonic(sl.m_strOp, eOp)) {
            EAddrMode eMode = AddrModeOf(eOp);
            if (eMode == EAddrMode::amRegMem) {
                // register-to-register LD takes 1 word
                if (eOp == EOp::opLD && sl.m_arrOperands.size() == 2 &&
                    RegisterIndex(Upper(sl.m_arrOperands[1])) >= 0)
                    nAddr += 1;
                else
                    nAddr += 2;
                // address operands may create literals
                for (size_t k = 1; k < sl.m_arrOperands.size(); ++k)
                    if (!sl.m_arrOperands[k].empty() &&
                        sl.m_arrOperands[k][0] == '#')
                        litPool.Add(sl.m_arrOperands[k]);
            } else if (eMode == EAddrMode::amMem) {
                nAddr += 2;
                if (!sl.m_arrOperands.empty() && !sl.m_arrOperands[0].empty() &&
                    sl.m_arrOperands[0][0] == '#')
                    litPool.Add(sl.m_arrOperands[0]);
            } else {
                nAddr += 1;
            }
            continue;
        }
        arrErrors.push_back({sl.m_nNo, "unknown instruction '" + sl.m_strOp + "'"});
    }

    // assign literal pool addresses
    int nLiteralBase = nAddr;
    for (const auto& strKey : litPool.m_arrOrder)
        litPool.m_mapLiterals[strKey] = nLiteralBase++;

    result.m_arrWords.assign(nLiteralBase, 0);
    result.m_mapSymbols = mapSymbols;

    // ---------------- pass 2: emit words ----------------------------------
    nAddr = 0;
    auto Emit = [&](uint16_t wWord, int nLineNo, bool bIsData) {
        if (nAddr < (int)result.m_arrWords.size()) result.m_arrWords[nAddr] = wWord;
        CCodeEntry entry;
        entry.m_nSrcLine = nLineNo;
        entry.m_nAddress = nAddr;
        entry.m_wWord0 = wWord;
        entry.m_bIsData = bIsData;
        result.m_arrEntries.push_back(entry);
        ++nAddr;
    };
    auto EmitOpWord = [&](EOp eOp, uint8_t byGr, uint8_t byXr, uint16_t wAddr,
                          int nLineNo) {
        // SVC encodes its function code in the low byte of the single word.
        uint16_t w = eOp == EOp::opSVC
                         ? (uint16_t)(((uint16_t)eOp << 8) | (wAddr & 0xFF))
                         : (uint16_t)(((uint16_t)eOp << 8) | (byGr << 4) | byXr);
        Emit(w, nLineNo, false);
        CCodeEntry& entry = result.m_arrEntries.back();
        entry.m_eOp = eOp;
        entry.m_byGr = byGr;
        entry.m_byXr = byXr;
        entry.m_nAddr = wAddr;
    };

    for (const auto& sl : arrLines) {
        if (sl.m_bEmpty || sl.m_strOp.empty() || sl.m_strOp == "START" ||
            sl.m_strOp == "END")
            continue;

        if (sl.m_strOp == "DS") {
            long long llN = 1;
            if (sl.m_arrOperands.empty() ||
                !ParseSignedInt(sl.m_arrOperands[0], llN) || llN < 0)
                llN = 1;
            for (long long i = 0; i < llN; ++i) Emit(0, sl.m_nNo, true);
            continue;
        }

        if (sl.m_strOp == "DC") {
            for (const auto& strConst : sl.m_arrOperands) {
                std::string str = Trim(strConst);
                if (str.empty()) continue;
                if (str[0] == '\'') {
                    for (size_t k = 1; k < str.size();) {
                        if (str[k] == '\'') {
                            if (k + 1 < str.size() && str[k + 1] == '\'') {
                                Emit('\'', sl.m_nNo, true);
                                k += 2;
                            } else break;
                        } else {
                            Emit((uint16_t)(unsigned char)str[k], sl.m_nNo, true);
                            k += 1;
                        }
                    }
                } else if (IsSymbolLike(str)) {
                    auto it = mapSymbols.find(Upper(str));
                    if (it != mapSymbols.end())
                        Emit((uint16_t)it->second, sl.m_nNo, true);
                    else
                        arrErrors.push_back({sl.m_nNo, "undefined symbol '" + str + "' in DC"});
                } else {
                    long long ll;
                    if (ParseSignedInt(str, ll) && ll >= -32768 && ll <= 65535)
                        Emit((uint16_t)(int16_t)ll, sl.m_nNo, true);
                    else
                        arrErrors.push_back({sl.m_nNo, "bad DC constant '" + str + "'"});
                }
            }
            continue;
        }

        if (sl.m_strOp == "IN" || sl.m_strOp == "OUT") {
            if (sl.m_arrOperands.size() != 2) {
                nAddr += 3;
                continue;
            }
            uint8_t byCode = sl.m_strOp == "IN"
                                 ? (uint8_t)ESvcCode::svcIN
                                 : (uint8_t)ESvcCode::svcOUT;
            EmitOpWord(EOp::opSVC, 0, 0, byCode, sl.m_nNo);
            for (int i = 0; i < 2; ++i) {
                int nValue = 0;
                if (EvalAddress(sl.m_arrOperands[i], sl.m_nNo, mapSymbols,
                                litPool, nValue, arrErrors, true))
                    Emit((uint16_t)nValue, sl.m_nNo, true);
                else
                    Emit(0, sl.m_nNo, true);
            }
            continue;
        }

        if (sl.m_strOp == "EXIT") {
            EmitOpWord(EOp::opSVC, 0, 0, (uint16_t)ESvcCode::svcEXIT, sl.m_nNo);
            continue;
        }

        if (sl.m_strOp == "SVC") {
            long long llCode = 0;
            if (sl.m_arrOperands.empty() ||
                !ParseSignedInt(sl.m_arrOperands[0], llCode))
                arrErrors.push_back({sl.m_nNo, "SVC needs an integer code"});
            EmitOpWord(EOp::opSVC, 0, 0, (uint16_t)(llCode & 0xFF), sl.m_nNo);
            continue;
        }

        EOp eOp;
        if (!LookupMnemonic(sl.m_strOp, eOp)) continue;
        EAddrMode eMode = AddrModeOf(eOp);

        uint8_t byGr = 0;
        if (eMode == EAddrMode::amRegMem || eMode == EAddrMode::amStack) {
            int nGr = sl.m_arrOperands.empty()
                          ? -1
                          : RegisterIndex(Upper(sl.m_arrOperands[0]));
            if (nGr < 0) {
                arrErrors.push_back(
                    {sl.m_nNo, "first operand of " + sl.m_strOp + " must be GR0..GR7"});
            } else {
                byGr = (uint8_t)nGr;
            }
        }

        switch (eMode) {
        case EAddrMode::amNone:
            EmitOpWord(eOp, 0, 0, 0, sl.m_nNo);
            break;
        case EAddrMode::amStack:
            EmitOpWord(eOp, byGr, 0, 0, sl.m_nNo);
            break;
        case EAddrMode::amRegMem: {
            if (eOp == EOp::opLD && sl.m_arrOperands.size() == 2 &&
                RegisterIndex(Upper(sl.m_arrOperands[1])) >= 0) {
                uint8_t bySrc =
                    (uint8_t)RegisterIndex(Upper(sl.m_arrOperands[1]));
                EmitOpWord(EOp::opLDR, byGr, bySrc, 0, sl.m_nNo);
                break;
            }
            int nAddrValue = 0;
            uint8_t byXr = 0;
            if (sl.m_arrOperands.size() < 2) {
                arrErrors.push_back(
                    {sl.m_nNo, sl.m_strOp + " needs an address operand"});
            } else {
                EvalAddress(sl.m_arrOperands[1], sl.m_nNo, mapSymbols, litPool,
                            nAddrValue, arrErrors, true);
                if (sl.m_arrOperands.size() > 2) {
                    int nX = RegisterIndex(Upper(sl.m_arrOperands[2]));
                    if (nX < 0)
                        arrErrors.push_back({sl.m_nNo, "third operand of " + sl.m_strOp + " must be an index register"});
                    else
                        byXr = (uint8_t)nX;
                }
            }
            EmitOpWord(eOp, byGr, byXr, (uint16_t)nAddrValue, sl.m_nNo);
            Emit((uint16_t)nAddrValue, sl.m_nNo, false);
            break;
        }
        case EAddrMode::amMem: {
            int nAddrValue = 0;
            uint8_t byXr = 0;
            if (sl.m_arrOperands.empty()) {
                arrErrors.push_back(
                    {sl.m_nNo, sl.m_strOp + " needs an address operand"});
            } else {
                EvalAddress(sl.m_arrOperands[0], sl.m_nNo, mapSymbols, litPool,
                            nAddrValue, arrErrors, true);
                if (sl.m_arrOperands.size() > 1) {
                    int nX = RegisterIndex(Upper(sl.m_arrOperands[1]));
                    if (nX < 0)
                        arrErrors.push_back({sl.m_nNo, "second operand of " + sl.m_strOp + " must be an index register"});
                    else
                        byXr = (uint8_t)nX;
                }
            }
            EmitOpWord(eOp, 0, byXr, (uint16_t)nAddrValue, sl.m_nNo);
            Emit((uint16_t)nAddrValue, sl.m_nNo, false);
            break;
        }
        case EAddrMode::amImm:
            EmitOpWord(eOp, 0, 0, 0, sl.m_nNo);
            break;
        }
    }

    // literal pool
    for (const auto& strKey : litPool.m_arrOrder) {
        long long ll = 0;
        if (strKey.size() > 1 && strKey[0] == '#' && ParseSignedInt(strKey, ll))
            Emit((uint16_t)(int16_t)ll, 0, true);
        else
            Emit(0, 0, true);
    }

    // entry address
    if (!strEntryLabel.empty()) {
        auto it = mapSymbols.find(strEntryLabel);
        if (it != mapSymbols.end()) result.m_nStartAddress = it->second;
        else arrErrors.push_back({0, "undefined entry label '" + strEntryLabel + "'"});
    }

    // build line map (instructions only, ascending)
    std::set<int> setSeenLines;
    for (const auto& entry : result.m_arrEntries)
        if (!entry.m_bIsData && entry.m_nSrcLine > 0 &&
            !setSeenLines.count(entry.m_nSrcLine)) {
            setSeenLines.insert(entry.m_nSrcLine);
            result.m_arrLineMap.push_back({entry.m_nSrcLine, entry.m_nAddress});
        }

    result.m_bOk = arrErrors.empty();
    return result;
}

} // namespace casl
