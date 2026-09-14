#include "casl/OpCode.hpp"

namespace casl {

const char* OpName(EOp eOp) {
    switch (eOp) {
    case EOp::opNOP:  return "NOP";
    case EOp::opLD:   return "LD";
    case EOp::opST:   return "ST";
    case EOp::opADDA: return "ADDA";
    case EOp::opADDL: return "ADDL";
    case EOp::opSUBA: return "SUBA";
    case EOp::opSUBL: return "SUBL";
    case EOp::opAND:  return "AND";
    case EOp::opOR:   return "OR";
    case EOp::opXOR:  return "XOR";
    case EOp::opCPA:  return "CPA";
    case EOp::opCPL:  return "CPL";
    case EOp::opSLA:  return "SLA";
    case EOp::opSRA:  return "SRA";
    case EOp::opSLL:  return "SLL";
    case EOp::opSRL:  return "SRL";
    case EOp::opJUMP: return "JUMP";
    case EOp::opJPL:  return "JPL";
    case EOp::opJMI:  return "JMI";
    case EOp::opJNZ:  return "JNZ";
    case EOp::opJZE:  return "JZE";
    case EOp::opJOV:  return "JOV";
    case EOp::opPUSH: return "PUSH";
    case EOp::opPOP:  return "POP";
    case EOp::opCALL: return "CALL";
    case EOp::opRET:  return "RET";
    case EOp::opLDR:  return "LD";
    case EOp::opSVC:  return "SVC";
    }
    return "???";
}

EAddrMode AddrModeOf(EOp eOp) {
    switch (eOp) {
    case EOp::opNOP:
    case EOp::opRET:
        return EAddrMode::amNone;
    case EOp::opSVC:
    case EOp::opLDR:
        return EAddrMode::amImm;
    case EOp::opJUMP:
    case EOp::opJPL:
    case EOp::opJMI:
    case EOp::opJNZ:
    case EOp::opJZE:
    case EOp::opJOV:
    case EOp::opPUSH:
    case EOp::opCALL:
        return EAddrMode::amMem;
    case EOp::opPOP:
        return EAddrMode::amStack;
    default:
        return EAddrMode::amRegMem;
    }
}

int RegisterIndex(const std::string& strName) {
    if (strName.size() != 3) return -1;
    if (strName[0] != 'G' && strName[0] != 'g') return -1;
    if (strName[1] != 'R' && strName[1] != 'r') return -1;
    char ch = strName[2];
    if (ch < '0' || ch > '7') return -1;
    return ch - '0';
}

std::string RegisterName(int nIndex) {
    nIndex &= 7;
    return std::string("GR") + char('0' + nIndex);
}

} // namespace casl
