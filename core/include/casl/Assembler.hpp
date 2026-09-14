// CASL II two-pass assembler. Pure C++17, no Qt dependency.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include "casl/OpCode.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace casl {

class CAssembleResult {
public:
    bool                                      m_bOk = false;
    std::vector<CAsmError>                    m_arrErrors;

    int                                       m_nStartAddress = 0;
    std::string                               m_strProgramName;
    std::vector<uint16_t>                     m_arrWords;   // image at address 0
    std::map<std::string, int>                m_mapSymbols; // label -> address
    std::vector<CCodeEntry>                   m_arrEntries;
    std::vector<std::pair<int, int>>          m_arrLineMap; // (srcLine, addr)

    bool HasError() const { return !m_arrErrors.empty(); }
};

CAssembleResult Assemble(const std::string& strSource);

// Parse an integer literal: "#1F" (hex), "42" (decimal), "-42", "+42".
// Returns false when the text is not a valid integer literal.
bool ParseIntLiteral(const std::string& strText, long long& llValue);

} // namespace casl
