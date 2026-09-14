// CASL/COMET II instruction set definitions.
// Pure C++17, no Qt dependency. Part of the casl-core library.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace casl {

// ---------------------------------------------------------------------------
// COMET II registers
//   GR0..GR7 : general registers (16-bit)
//   SP       : stack pointer
//   PR       : program counter
//   FR       : flags (OF/SF/ZF)
// Memory is 65536 words of 16 bits, addresses wrap modulo 65536.
// ---------------------------------------------------------------------------

constexpr int MEMORY_WORDS  = 65536;
constexpr int REGISTER_COUNT = 8;
constexpr uint16_t INITIAL_SP = 0xFFFE;

enum class EFlag : uint8_t { flOF = 0, flSF = 1, flZF = 2 };

// Instruction op-codes (COMET II). The numeric values only need to be
// self-consistent between the assembler and the machine.
enum class EOp : uint8_t {
    opNOP  = 0x00,
    opLD   = 0x10,  // load
    opST   = 0x11,  // store
    opADDA = 0x20,  // add arithmetic      (signed)
    opADDL = 0x21,  // add logical         (unsigned)
    opSUBA = 0x22,  // subtract arithmetic (signed)
    opSUBL = 0x23,  // subtract logical    (unsigned)
    opAND  = 0x24,
    opOR   = 0x25,
    opXOR  = 0x26,
    opCPA  = 0x30,  // compare arithmetic
    opCPL  = 0x31,  // compare logical
    opSLA  = 0x32,  // shift left arithmetic
    opSRA  = 0x33,  // shift right arithmetic
    opSLL  = 0x34,  // shift left logical
    opSRL  = 0x35,  // shift right logical
    opJUMP = 0x40,
    opJPL  = 0x41,  // jump if plus
    opJMI  = 0x42,  // jump if minus
    opJNZ  = 0x43,  // jump if not zero
    opJZE  = 0x44,  // jump if zero
    opJOV  = 0x45,  // jump if overflow
    opPUSH = 0x50,
    opPOP  = 0x51,
    opCALL = 0x52,
    opRET  = 0x53,
    opLDR  = 0x14,  // register-to-register LD (assembler pseudo-encoding)
    opSVC  = 0x70,  // supervisor call: 0=EXIT, 1=IN, 2=OUT
};

// SVC function codes
enum class ESvcCode : uint8_t { svcEXIT = 0, svcIN = 1, svcOUT = 2 };

const char* OpName(EOp eOp);

// Operand addressing style used to decode/encode an instruction.
enum class EAddrMode : uint8_t {
    amNone,    // NOP, RET
    amImm,     // single word op|code, used by SVC / register LD
    amMem,     // addr[,index]                  : JUMP/PUSH/CALL family
    amRegMem,  // GR,addr[,index]               : most arithmetic
    amRegReg,  // GR,GR                         : register LD
    amStack,   // GR                            : POP
};

EAddrMode AddrModeOf(EOp eOp);

// One assembled instruction, in a format friendly for disassembly views.
class CCodeEntry {
public:
    int      m_nSrcLine = 0;   // source line that produced this word
    int      m_nAddress = 0;
    EOp      m_eOp = EOp::opNOP;
    uint8_t  m_byGr = 0;
    uint8_t  m_byXr = 0;
    int      m_nAddr = 0;
    uint16_t m_wWord0 = 0;
    uint16_t m_wWord1 = 0;
    bool     m_bTwoWords = false;
    bool     m_bIsData = false;  // DC/DS/literal words
};

class CAsmError {
public:
    int         m_nLine = 0;      // 1-based source line, 0 = not tied to a line
    std::string m_strMessage;
};

// Register name <-> index ("GR0".."GR7"). Returns -1 when invalid.
int RegisterIndex(const std::string& strName);
std::string RegisterName(int nIndex);

} // namespace casl
